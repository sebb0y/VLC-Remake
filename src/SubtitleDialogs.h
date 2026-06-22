#pragma once

#include <QDialog>

#include "SubtitleManager.h"

class QTableWidget;
class QLineEdit;

// A dialog that lists the subtitles found for a video and lets the user pick
// one. Hash-matched (sync-guaranteed) results are highlighted.
class SubtitleSearchDialog : public QDialog {
    Q_OBJECT
public:
    SubtitleSearchDialog(const QList<SubtitleManager::Result> &results,
                         QWidget *parent = nullptr);

    // Index into the results list, or -1 if cancelled.
    int selectedIndex() const { return m_selected; }

private:
    int m_selected = -1;
    QTableWidget *m_table = nullptr;
};

// A small settings dialog for the OpenSubtitles API key, optional account
// credentials, and preferred subtitle languages. Persists via QSettings.
class SubtitleSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SubtitleSettingsDialog(QWidget *parent = nullptr);

private slots:
    void save();

private:
    QLineEdit *m_apiKey = nullptr;
    QLineEdit *m_username = nullptr;
    QLineEdit *m_password = nullptr;
    QLineEdit *m_languages = nullptr;
};
