#include "SubtitleDialogs.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

// ----------------------------- search dialog --------------------------------

SubtitleSearchDialog::SubtitleSearchDialog(
    const QList<SubtitleManager::Result> &results, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle(tr("Download Subtitles"));
    resize(640, 380);

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(
        tr("Found %1 subtitle(s). A ✓ in the <b>Sync</b> column means the "
           "subtitle was matched to this exact video file and should line up "
           "perfectly. Pick one and click Download.")
            .arg(results.size()),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_table = new QTableWidget(results.size(), 4, this);
    m_table->setHorizontalHeaderLabels(
        {tr("Sync"), tr("Lang"), tr("Release / File"), tr("Downloads")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    for (int row = 0; row < results.size(); ++row) {
        const SubtitleManager::Result &r = results.at(row);
        QString label = r.releaseName;
        if (r.hearingImpaired)
            label += tr("  [hearing impaired]");

        auto *sync = new QTableWidgetItem(r.hashMatch ? "✓" : "");
        sync->setTextAlignment(Qt::AlignCenter);
        if (r.hashMatch)
            sync->setToolTip(tr("Matched to this file — timing is reliable."));

        m_table->setItem(row, 0, sync);
        m_table->setItem(row, 1, new QTableWidgetItem(r.language));
        m_table->setItem(row, 2, new QTableWidgetItem(label));
        m_table->setItem(row, 3,
                         new QTableWidgetItem(QString::number(r.downloadCount)));
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->selectRow(0);
    layout->addWidget(m_table);

    auto *buttons = new QDialogButtonBox(this);
    auto *downloadButton = buttons->addButton(tr("Download"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    downloadButton->setDefault(true);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        m_selected = m_table->currentRow();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this]() {
        m_selected = m_table->currentRow();
        accept();
    });
}

// ---------------------------- settings dialog -------------------------------

SubtitleSettingsDialog::SubtitleSettingsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("OpenSubtitles Account"));
    setMinimumWidth(440);

    QSettings settings;
    m_apiKey = new QLineEdit(settings.value("opensubtitles/apiKey").toString(), this);
    m_username = new QLineEdit(settings.value("opensubtitles/username").toString(), this);
    m_password = new QLineEdit(settings.value("opensubtitles/password").toString(), this);
    m_password->setEchoMode(QLineEdit::Password);
    m_languages = new QLineEdit(
        settings.value("subtitles/languages", "en").toString(), this);

    auto *info = new QLabel(
        tr("Subtitle search uses OpenSubtitles.com (free). Create an account "
           "there, then go to <b>API → Consumers</b> to generate an API key. "
           "Your username and password are needed to download.<br><br>"
           "Languages: comma-separated codes like <tt>en,es,fr</tt>."),
        this);
    info->setWordWrap(true);
    info->setOpenExternalLinks(true);

    auto *form = new QFormLayout;
    form->addRow(tr("API key:"), m_apiKey);
    form->addRow(tr("Username:"), m_username);
    form->addRow(tr("Password:"), m_password);
    form->addRow(tr("Languages:"), m_languages);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SubtitleSettingsDialog::save);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(info);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void SubtitleSettingsDialog::save() {
    QSettings settings;
    settings.setValue("opensubtitles/apiKey", m_apiKey->text().trimmed());
    settings.setValue("opensubtitles/username", m_username->text().trimmed());
    settings.setValue("opensubtitles/password", m_password->text());
    QString langs = m_languages->text().trimmed();
    settings.setValue("subtitles/languages", langs.isEmpty() ? "en" : langs);
    accept();
}
