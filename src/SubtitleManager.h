#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

// SubtitleManager talks to the OpenSubtitles REST API to find and download
// subtitles for a video.
//
// Sync strategy: the most reliable matches come from OpenSubtitles' "movie
// hash" — a fingerprint computed from the video file's size and its first and
// last 64 KB. Subtitles returned via a hash match were timed against that
// exact release, so they line up without adjustment. We also do a looser
// filename query as a fallback; those are flagged so the user knows they may
// need the delay nudge.
class SubtitleManager : public QObject {
    Q_OBJECT
public:
    struct Result {
        QString language;
        QString releaseName;
        QString fileName;
        qint64 fileId = 0;
        int downloadCount = 0;
        bool hashMatch = false;
        bool hearingImpaired = false;
    };

    explicit SubtitleManager(QObject *parent = nullptr);

    // True if an OpenSubtitles API key has been configured.
    bool isConfigured() const;

    // Kick off a search for the given local video file.
    void searchForVideo(const QString &videoPath);

    // Download the chosen subtitle and save it next to the video file.
    void download(const Result &result, const QString &videoPath);

    // Compute the OpenSubtitles movie hash for a file. Returns empty on error.
    static QString computeMovieHash(const QString &path);

signals:
    void searchStarted();
    void searchFinished(const QList<SubtitleManager::Result> &results);
    void failed(const QString &message);
    void downloadFinished(const QString &savedPath);

private:
    QString apiKey() const;
    QString username() const;
    QString password() const;
    QStringList languages() const;

    void sendSearch(const QString &moviehash, const QString &query);
    void parseSearchReply(QNetworkReply *reply, bool wasHashSearch);
    void ensureLoginThen(const std::function<void()> &next);
    void requestDownloadLink(qint64 fileId, const QString &videoPath);
    void fetchSubtitleFile(const QString &url, const QString &videoPath,
                           const QString &suggestedName);

    QNetworkAccessManager *m_net = nullptr;
    QString m_token;          // JWT from /login, used for downloads
    QString m_downloadBase;   // base host returned by /login

    // Carried between the hash search and the fallback query search.
    QString m_pendingVideoPath;
    QString m_pendingQuery;
    QList<Result> m_accumulated;
};
