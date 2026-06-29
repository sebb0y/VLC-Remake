#pragma once

#include <QMainWindow>
#include <QStringList>

class MpvWidget;
class PlayerControls;
class SubtitleManager;
class QListWidget;
class QDockWidget;
class QStackedWidget;
class QAction;

// MainWindow ties together the video surface, the transport controls, the
// playlist and all the menus / keyboard shortcuts. It is the only place that
// knows about playlist state and the current track.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Entry point for files/URLs passed on the command line or via the OS
    // when opening a video with this app.
    void openPaths(const QStringList &paths);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void openFiles();
    void openUrl();
    void playNext();
    void playPrevious();
    void toggleFullScreen();
    void showAbout();
    void downloadSubtitles();
    void addSubtitleFile();
    void editSubtitleSettings();

private:
    void buildMenus();
    void buildLayout();
    QWidget *buildWelcomePage(QWidget *parent);
    void wireConnections();
    void addToPlaylist(const QStringList &paths);
    void playIndex(int index);
    QString currentVideoPath() const;

    // Auto-hiding control bar.
    void showControlsBar();
    void hideControlsBar();
    void onIdleTick();

    MpvWidget *m_player = nullptr;
    PlayerControls *m_controls = nullptr;
    SubtitleManager *m_subtitles = nullptr;
    QListWidget *m_playlist = nullptr;
    QDockWidget *m_playlistDock = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_welcome = nullptr;
    QTimer *m_idleTimer = nullptr;

    QStringList m_items;
    int m_currentIndex = -1;
    bool m_wasMaximized = false;
    bool m_playbackStarted = false;
    bool m_paused = false;
    QPoint m_lastCursorPos;
    int m_idleMs = 0;
};
