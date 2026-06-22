#include "SubtitleManager.h"

#include <functional>

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

namespace {
const char *kApiHost = "https://api.opensubtitles.com/api/v1";
const char *kUserAgent = "Vela v1.0";

void applyHeaders(QNetworkRequest &request, const QString &apiKey,
                  const QString &token = QString()) {
    request.setRawHeader("Api-Key", apiKey.toUtf8());
    request.setRawHeader("User-Agent", kUserAgent);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    if (!token.isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
}
} // namespace

SubtitleManager::SubtitleManager(QObject *parent) : QObject(parent) {
    m_net = new QNetworkAccessManager(this);
}

QString SubtitleManager::apiKey() const {
    return QSettings().value("opensubtitles/apiKey").toString().trimmed();
}

QString SubtitleManager::username() const {
    return QSettings().value("opensubtitles/username").toString().trimmed();
}

QString SubtitleManager::password() const {
    return QSettings().value("opensubtitles/password").toString();
}

QStringList SubtitleManager::languages() const {
    QString raw = QSettings().value("subtitles/languages", "en").toString();
    QStringList langs;
    for (const QString &part : raw.split(',', Qt::SkipEmptyParts))
        langs << part.trimmed().toLower();
    if (langs.isEmpty())
        langs << "en";
    return langs;
}

bool SubtitleManager::isConfigured() const {
    return !apiKey().isEmpty();
}

QString SubtitleManager::computeMovieHash(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const qint64 size = file.size();
    const int chunk = 65536; // 64 KB
    if (size < 2 * chunk)
        return {}; // file too small for the algorithm

    quint64 hash = static_cast<quint64>(size);

    auto addChunk = [&](const QByteArray &bytes) {
        const int count = bytes.size() / 8;
        const auto *data = reinterpret_cast<const uchar *>(bytes.constData());
        for (int i = 0; i < count; ++i) {
            quint64 value = 0;
            for (int b = 0; b < 8; ++b) // little-endian, per the spec
                value |= static_cast<quint64>(data[i * 8 + b]) << (8 * b);
            hash += value; // intentional 64-bit wraparound
        }
    };

    QByteArray head = file.read(chunk);
    if (!file.seek(size - chunk))
        return {};
    QByteArray tail = file.read(chunk);
    if (head.size() < chunk || tail.size() < chunk)
        return {};

    addChunk(head);
    addChunk(tail);

    return QString("%1").arg(hash, 16, 16, QChar('0')).toLower();
}

void SubtitleManager::searchForVideo(const QString &videoPath) {
    if (apiKey().isEmpty()) {
        emit failed(tr("No OpenSubtitles API key set. Open Subtitles → "
                       "OpenSubtitles Account… to add a free key."));
        return;
    }

    QFileInfo info(videoPath);
    if (!info.exists()) {
        emit failed(tr("Subtitle search needs a local video file."));
        return;
    }

    emit searchStarted();

    m_pendingVideoPath = videoPath;
    // Clean the filename into a search query: drop the extension and turn
    // separators into spaces so the fallback text search behaves.
    QString query = info.completeBaseName();
    query.replace(QRegularExpression("[._]+"), " ");
    m_pendingQuery = query.simplified();
    m_accumulated.clear();

    const QString hash = computeMovieHash(videoPath);
    sendSearch(hash, QString());
}

void SubtitleManager::sendSearch(const QString &moviehash, const QString &query) {
    QUrl url(QString(kApiHost) + "/subtitles");
    QUrlQuery params;
    params.addQueryItem("languages", languages().join(','));
    if (!moviehash.isEmpty())
        params.addQueryItem("moviehash", moviehash);
    if (!query.isEmpty())
        params.addQueryItem("query", query);
    url.setQuery(params);

    QNetworkRequest request(url);
    applyHeaders(request, apiKey());

    const bool wasHashSearch = !moviehash.isEmpty();
    QNetworkReply *reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, wasHashSearch]() {
        parseSearchReply(reply, wasHashSearch);
    });
}

void SubtitleManager::parseSearchReply(QNetworkReply *reply, bool wasHashSearch) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // A failed hash search shouldn't abort everything — fall through to
        // the filename query so the user still gets options.
        if (wasHashSearch) {
            sendSearch(QString(), m_pendingQuery);
            return;
        }
        emit failed(tr("Subtitle search failed: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonArray data = doc.object().value("data").toArray();

    for (const QJsonValue &item : data) {
        const QJsonObject attrs = item.toObject().value("attributes").toObject();
        const QJsonArray files = attrs.value("files").toArray();
        if (files.isEmpty())
            continue;

        Result r;
        r.language = attrs.value("language").toString().toUpper();
        r.releaseName = attrs.value("release").toString();
        r.downloadCount = attrs.value("download_count").toInt();
        r.hearingImpaired = attrs.value("hearing_impaired").toBool();
        r.hashMatch = attrs.value("moviehash_match").toBool();
        const QJsonObject f = files.first().toObject();
        r.fileId = static_cast<qint64>(f.value("file_id").toDouble());
        r.fileName = f.value("file_name").toString();
        if (r.releaseName.isEmpty())
            r.releaseName = r.fileName;

        // De-duplicate across the two searches.
        bool seen = false;
        for (const Result &existing : m_accumulated) {
            if (existing.fileId == r.fileId) {
                seen = true;
                break;
            }
        }
        if (!seen)
            m_accumulated.append(r);
    }

    if (wasHashSearch) {
        // Now broaden with a filename query for more language choices.
        sendSearch(QString(), m_pendingQuery);
        return;
    }

    // Hash matches first (sync-guaranteed), then most-downloaded.
    std::sort(m_accumulated.begin(), m_accumulated.end(),
              [](const Result &a, const Result &b) {
                  if (a.hashMatch != b.hashMatch)
                      return a.hashMatch;
                  return a.downloadCount > b.downloadCount;
              });

    if (m_accumulated.isEmpty())
        emit failed(tr("No subtitles found for this video."));
    else
        emit searchFinished(m_accumulated);
}

void SubtitleManager::ensureLoginThen(const std::function<void()> &next) {
    if (!m_token.isEmpty() || username().isEmpty() || password().isEmpty()) {
        // Either already logged in, or no credentials to log in with — try
        // the download with just the API key.
        next();
        return;
    }

    QNetworkRequest request(QUrl(QString(kApiHost) + "/login"));
    applyHeaders(request, apiKey());

    QJsonObject body;
    body.insert("username", username());
    body.insert("password", password());
    QNetworkReply *reply = m_net->post(request, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, next]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
            m_token = obj.value("token").toString();
            const QString base = obj.value("base_url").toString();
            if (!base.isEmpty())
                m_downloadBase = "https://" + base + "/api/v1";
        }
        // Even if login failed, attempt the download with the API key alone.
        next();
    });
}

void SubtitleManager::download(const Result &result, const QString &videoPath) {
    ensureLoginThen([this, result, videoPath]() {
        requestDownloadLink(result.fileId, videoPath);
    });
}

void SubtitleManager::requestDownloadLink(qint64 fileId, const QString &videoPath) {
    const QString host = m_downloadBase.isEmpty() ? kApiHost : m_downloadBase;
    QNetworkRequest request(QUrl(host + "/download"));
    applyHeaders(request, apiKey(), m_token);

    QJsonObject body;
    body.insert("file_id", static_cast<double>(fileId));
    QNetworkReply *reply = m_net->post(request, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, videoPath]() {
        reply->deleteLater();
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        if (reply->error() != QNetworkReply::NoError) {
            QString detail = obj.value("message").toString();
            if (detail.isEmpty())
                detail = reply->errorString();
            emit failed(tr("Could not get a download link: %1\n"
                           "Free downloads require an OpenSubtitles account — "
                           "add your username/password in OpenSubtitles Account…")
                            .arg(detail));
            return;
        }
        const QString link = obj.value("link").toString();
        const QString name = obj.value("file_name").toString();
        if (link.isEmpty()) {
            emit failed(tr("OpenSubtitles did not return a download link "
                           "(daily quota may be exhausted)."));
            return;
        }
        fetchSubtitleFile(link, videoPath, name);
    });
}

void SubtitleManager::fetchSubtitleFile(const QString &url, const QString &videoPath,
                                        const QString &suggestedName) {
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("User-Agent", kUserAgent);
    QNetworkReply *reply = m_net->get(request);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, videoPath, suggestedName]() {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    emit failed(tr("Downloading the subtitle file failed: %1")
                                    .arg(reply->errorString()));
                    return;
                }

                // Save alongside the video as "<video>.<ext>" so mpv keeps it
                // associated and the user can reuse it later.
                QFileInfo info(videoPath);
                QString ext = QFileInfo(suggestedName).suffix();
                if (ext.isEmpty())
                    ext = "srt";
                const QString target =
                    info.absolutePath() + "/" + info.completeBaseName() + "." + ext;

                QFile out(target);
                if (!out.open(QIODevice::WriteOnly)) {
                    emit failed(tr("Could not save subtitle to %1").arg(target));
                    return;
                }
                out.write(reply->readAll());
                out.close();
                emit downloadFinished(target);
            });
}
