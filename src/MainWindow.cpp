#include "MainWindow.h"

#include "MpvWidget.h"
#include "PlayerControls.h"
#include "SubtitleDialogs.h"
#include "SubtitleManager.h"

#include <QApplication>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPixmap>
#include <QStackedWidget>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {
const char *kVideoFilter =
    "Video files (*.mp4 *.mkv *.avi *.mov *.webm *.flv *.wmv *.m4v *.mpg "
    "*.mpeg *.3gp *.ts *.m2ts *.ogv *.vob);;All files (*)";
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(tr("Vela Media Player"));
    setMinimumSize(720, 480);
    setAcceptDrops(true);
    setWindowIcon(QIcon(":/icons/vela.svg"));

    m_subtitles = new SubtitleManager(this);

    buildLayout();
    buildMenus();
    wireConnections();

    // Apply the initial volume the slider starts at.
    m_player->setVolume(100);
    statusBar()->showMessage(tr("Open a video to begin — drag & drop works too."));
}

void MainWindow::buildLayout() {
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_player = new MpvWidget(central);
    m_controls = new PlayerControls(central);

    // A branded welcome screen sits in front of the video surface until the
    // first file is played. The logo is white line-art, so it shows cleanly
    // on the dark background; we swap to the actual video once playback starts.
    m_welcome = buildWelcomePage(central);

    m_stack = new QStackedWidget(central);
    m_stack->addWidget(m_welcome);
    m_stack->addWidget(m_player);
    m_stack->setCurrentWidget(m_welcome);

    layout->addWidget(m_stack, 1);
    layout->addWidget(m_controls, 0);
    setCentralWidget(central);

    m_playlist = new QListWidget(this);
    m_playlistDock = new QDockWidget(tr("Playlist"), this);
    m_playlistDock->setWidget(m_playlist);
    m_playlistDock->setObjectName("playlistDock");
    addDockWidget(Qt::RightDockWidgetArea, m_playlistDock);
}

QWidget *MainWindow::buildWelcomePage(QWidget *parent) {
    auto *page = new QWidget(parent);
    page->setAutoFillBackground(true);
    QPalette pal = page->palette();
    pal.setColor(QPalette::Window, QColor("#1c1c22"));
    page->setPalette(pal);

    auto *logo = new QLabel(page);
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(":/icons/vela-logo.png"));
    logo->setScaledContents(false);

    auto *hint = new QLabel(
        tr("Open a video (Ctrl+O) or drag a file here to start"), page);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #8a8a92; font-size: 15px;");

    auto *layout = new QVBoxLayout(page);
    layout->addStretch(2);
    layout->addWidget(logo, 0, Qt::AlignCenter);
    layout->addSpacing(16);
    layout->addWidget(hint, 0, Qt::AlignCenter);
    layout->addStretch(3);
    return page;
}

void MainWindow::buildMenus() {
    QMenu *fileMenu = menuBar()->addMenu(tr("&Media"));
    QAction *openAction = fileMenu->addAction(tr("&Open File(s)..."), this, &MainWindow::openFiles);
    openAction->setShortcut(QKeySequence::Open);
    fileMenu->addAction(tr("Open &URL / Network Stream..."), this, &MainWindow::openUrl);
    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(tr("&Quit"), this, &QWidget::close);
    quitAction->setShortcut(QKeySequence::Quit);

    QMenu *playbackMenu = menuBar()->addMenu(tr("&Playback"));
    auto addShortcutAction = [&](QMenu *menu, const QString &text,
                                 const QKeySequence &keys, auto slot) {
        QAction *action = menu->addAction(text);
        action->setShortcut(keys);
        action->setShortcutContext(Qt::ApplicationShortcut);
        connect(action, &QAction::triggered, this, slot);
        return action;
    };

    addShortcutAction(playbackMenu, tr("Play / Pause"), Qt::Key_Space,
                      [this]() { m_player->togglePause(); });
    addShortcutAction(playbackMenu, tr("Stop"), Qt::Key_S,
                      [this]() { m_player->stop(); });
    playbackMenu->addSeparator();
    addShortcutAction(playbackMenu, tr("Previous"), Qt::Key_P, &MainWindow::playPrevious);
    addShortcutAction(playbackMenu, tr("Next"), Qt::Key_N, &MainWindow::playNext);
    playbackMenu->addSeparator();
    addShortcutAction(playbackMenu, tr("Jump +10s"), Qt::Key_Right,
                      [this]() { m_player->seekRelative(10); });
    addShortcutAction(playbackMenu, tr("Jump -10s"), Qt::Key_Left,
                      [this]() { m_player->seekRelative(-10); });
    addShortcutAction(playbackMenu, tr("Jump +60s"), Qt::SHIFT | Qt::Key_Right,
                      [this]() { m_player->seekRelative(60); });
    addShortcutAction(playbackMenu, tr("Jump -60s"), Qt::SHIFT | Qt::Key_Left,
                      [this]() { m_player->seekRelative(-60); });

    QMenu *audioMenu = menuBar()->addMenu(tr("&Audio"));
    addShortcutAction(audioMenu, tr("Volume Up"), Qt::Key_Up, [this]() {
        m_player->setVolume(qMin(130, static_cast<int>(m_player->propertyDouble("volume")) + 5));
    });
    addShortcutAction(audioMenu, tr("Volume Down"), Qt::Key_Down, [this]() {
        m_player->setVolume(qMax(0, static_cast<int>(m_player->propertyDouble("volume")) - 5));
    });
    addShortcutAction(audioMenu, tr("Mute"), Qt::Key_M,
                      [this]() { m_player->toggleMute(); });

    QMenu *videoMenu = menuBar()->addMenu(tr("&Video"));
    addShortcutAction(videoMenu, tr("Fullscreen"), Qt::Key_F, &MainWindow::toggleFullScreen);
    videoMenu->addAction(tr("Toggle Playlist"), this,
                         [this]() { m_playlistDock->setVisible(!m_playlistDock->isVisible()); });

    QMenu *subMenu = menuBar()->addMenu(tr("&Subtitles"));
    QAction *dlSubs = subMenu->addAction(tr("&Download Subtitles..."), this,
                                         &MainWindow::downloadSubtitles);
    dlSubs->setShortcut(Qt::CTRL | Qt::Key_D);
    subMenu->addAction(tr("&Add Subtitle File..."), this, &MainWindow::addSubtitleFile);
    subMenu->addSeparator();
    addShortcutAction(subMenu, tr("Toggle Subtitles On/Off"), Qt::Key_V,
                      [this]() { m_player->toggleSubtitleVisibility(); });
    addShortcutAction(subMenu, tr("Cycle Subtitle Track"), Qt::Key_J,
                      [this]() { m_player->cycleSubtitleTrack(); });
    subMenu->addSeparator();
    addShortcutAction(subMenu, tr("Subtitle Delay  +0.1s (later)"), Qt::SHIFT | Qt::Key_Z,
                      [this]() {
                          m_player->adjustSubtitleDelay(0.1);
                          statusBar()->showMessage(
                              tr("Subtitle delay: %1 s")
                                  .arg(m_player->propertyDouble("sub-delay"), 0, 'f', 1),
                              2000);
                      });
    addShortcutAction(subMenu, tr("Subtitle Delay  -0.1s (earlier)"), Qt::Key_Z,
                      [this]() {
                          m_player->adjustSubtitleDelay(-0.1);
                          statusBar()->showMessage(
                              tr("Subtitle delay: %1 s")
                                  .arg(m_player->propertyDouble("sub-delay"), 0, 'f', 1),
                              2000);
                      });
    subMenu->addSeparator();
    subMenu->addAction(tr("OpenSubtitles Account..."), this,
                       &MainWindow::editSubtitleSettings);

    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About Vela"), this, &MainWindow::showAbout);

    // Escape always leaves fullscreen.
    auto *escAction = new QAction(this);
    escAction->setShortcut(Qt::Key_Escape);
    escAction->setShortcutContext(Qt::ApplicationShortcut);
    connect(escAction, &QAction::triggered, this, [this]() {
        if (isFullScreen())
            toggleFullScreen();
    });
    addAction(escAction);
}

void MainWindow::wireConnections() {
    connect(m_controls, &PlayerControls::playPauseClicked, m_player, &MpvWidget::togglePause);
    connect(m_controls, &PlayerControls::stopClicked, m_player, &MpvWidget::stop);
    connect(m_controls, &PlayerControls::previousClicked, this, &MainWindow::playPrevious);
    connect(m_controls, &PlayerControls::nextClicked, this, &MainWindow::playNext);
    connect(m_controls, &PlayerControls::muteClicked, m_player, &MpvWidget::toggleMute);
    connect(m_controls, &PlayerControls::fullscreenClicked, this, &MainWindow::toggleFullScreen);
    connect(m_controls, &PlayerControls::seekRequested, m_player, &MpvWidget::seekAbsolute);
    connect(m_controls, &PlayerControls::volumeChanged, m_player, &MpvWidget::setVolume);

    connect(m_player, &MpvWidget::positionChanged, m_controls, &PlayerControls::setPosition);
    connect(m_player, &MpvWidget::durationChanged, m_controls, &PlayerControls::setDuration);
    connect(m_player, &MpvWidget::volumeChanged, m_controls, &PlayerControls::setVolume);
    connect(m_player, &MpvWidget::muteChanged, m_controls, &PlayerControls::setMuted);
    connect(m_player, &MpvWidget::pauseChanged, this,
            [this](bool paused) { m_controls->setPlaying(!paused); });
    connect(m_player, &MpvWidget::endFile, this, &MainWindow::playNext);
    connect(m_player, &MpvWidget::mediaTitleChanged, this, [this](const QString &title) {
        if (!title.isEmpty())
            setWindowTitle(title + " — " + tr("Vela"));
    });

    connect(m_playlist, &QListWidget::itemActivated, this, [this](QListWidgetItem *) {
        playIndex(m_playlist->currentRow());
    });

    connect(m_subtitles, &SubtitleManager::searchStarted, this, [this]() {
        statusBar()->showMessage(tr("Searching OpenSubtitles..."));
    });
    connect(m_subtitles, &SubtitleManager::failed, this, [this](const QString &message) {
        statusBar()->clearMessage();
        QMessageBox::warning(this, tr("Subtitles"), message);
    });
    connect(m_subtitles, &SubtitleManager::searchFinished, this,
            [this](const QList<SubtitleManager::Result> &results) {
                statusBar()->clearMessage();
                SubtitleSearchDialog dialog(results, this);
                if (dialog.exec() == QDialog::Accepted && dialog.selectedIndex() >= 0) {
                    statusBar()->showMessage(tr("Downloading subtitle..."));
                    m_subtitles->download(results.at(dialog.selectedIndex()),
                                          currentVideoPath());
                }
            });
    connect(m_subtitles, &SubtitleManager::downloadFinished, this,
            [this](const QString &path) {
                m_player->addSubtitle(path);
                statusBar()->showMessage(
                    tr("Subtitles loaded. Use Z / Shift+Z if they need nudging."), 6000);
            });
}

QString MainWindow::currentVideoPath() const {
    if (m_currentIndex < 0 || m_currentIndex >= m_items.size())
        return {};
    return m_items.at(m_currentIndex);
}

void MainWindow::downloadSubtitles() {
    const QString path = currentVideoPath();
    if (path.isEmpty() || path.contains("://")) {
        QMessageBox::information(
            this, tr("Subtitles"),
            tr("Play a local video file first, then search for its subtitles."));
        return;
    }
    if (!m_subtitles->isConfigured()) {
        QMessageBox::information(
            this, tr("Subtitles"),
            tr("First add a free OpenSubtitles API key under "
               "Subtitles → OpenSubtitles Account."));
        editSubtitleSettings();
        if (!m_subtitles->isConfigured())
            return;
    }
    m_subtitles->searchForVideo(path);
}

void MainWindow::addSubtitleFile() {
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Add Subtitle File"), QString(),
        tr("Subtitle files (*.srt *.ass *.ssa *.sub *.vtt);;All files (*)"));
    if (!file.isEmpty()) {
        m_player->addSubtitle(file);
        statusBar()->showMessage(tr("Subtitle added."), 4000);
    }
}

void MainWindow::editSubtitleSettings() {
    SubtitleSettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::openFiles() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Open Video Files"), QString(), tr(kVideoFilter));
    if (!files.isEmpty())
        openPaths(files);
}

void MainWindow::openUrl() {
    bool ok = false;
    const QString url = QInputDialog::getText(
        this, tr("Open Network Stream"),
        tr("Enter a media URL (http, https, rtsp, ...):"), QLineEdit::Normal,
        QString(), &ok);
    if (ok && !url.trimmed().isEmpty())
        openPaths({url.trimmed()});
}

void MainWindow::openPaths(const QStringList &paths) {
    if (paths.isEmpty())
        return;
    const int firstNew = m_items.size();
    addToPlaylist(paths);
    playIndex(firstNew);
}

void MainWindow::addToPlaylist(const QStringList &paths) {
    for (const QString &path : paths) {
        m_items.append(path);
        const bool isUrl = path.contains("://");
        const QString label = isUrl ? path : QFileInfo(path).fileName();
        m_playlist->addItem(label);
    }
}

void MainWindow::playIndex(int index) {
    if (index < 0 || index >= m_items.size())
        return;
    m_currentIndex = index;
    m_playlist->setCurrentRow(index);
    m_stack->setCurrentWidget(m_player); // leave the welcome screen
    m_player->loadFile(m_items.at(index));
    statusBar()->showMessage(tr("Playing: %1").arg(m_items.at(index)), 4000);
}

void MainWindow::playNext() {
    if (m_currentIndex + 1 < m_items.size())
        playIndex(m_currentIndex + 1);
}

void MainWindow::playPrevious() {
    if (m_currentIndex > 0)
        playIndex(m_currentIndex - 1);
}

void MainWindow::toggleFullScreen() {
    if (isFullScreen()) {
        showNormal();
        if (m_wasMaximized)
            showMaximized();
        menuBar()->show();
        statusBar()->show();
    } else {
        m_wasMaximized = isMaximized();
        menuBar()->hide();
        statusBar()->hide();
        showFullScreen();
    }
}

void MainWindow::showAbout() {
    QMessageBox::about(
        this, tr("About Vela"),
        tr("<h3>Vela Media Player</h3>"
           "<p>A lightweight, VLC-inspired video player.</p>"
           "<p>Built with Qt 6 and powered by libmpv / FFmpeg, so it plays "
           "MP4, MKV, AVI, MOV, WebM and just about anything else.</p>"
           "<p>Keyboard: Space play/pause, F fullscreen, arrows seek, "
           "Up/Down volume, M mute, N/P next/previous, S stop.</p>"));
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event) {
    QStringList paths;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls)
        paths << (url.isLocalFile() ? url.toLocalFile() : url.toString());
    if (!paths.isEmpty())
        openPaths(paths);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // Double-safety for fullscreen exit even if a child grabbed focus.
    if (event->key() == Qt::Key_Escape && isFullScreen()) {
        toggleFullScreen();
        return;
    }
    QMainWindow::keyPressEvent(event);
}
