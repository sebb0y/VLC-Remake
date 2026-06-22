#include "PlayerControls.h"

#include <cmath>

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QStyle>
#include <QToolButton>

static QToolButton *makeButton(QWidget *parent, QStyle::StandardPixmap icon,
                               const QString &tip) {
    auto *button = new QToolButton(parent);
    button->setIcon(parent->style()->standardIcon(icon));
    button->setToolTip(tip);
    button->setAutoRaise(true);
    button->setIconSize(QSize(22, 22));
    return button;
}

PlayerControls::PlayerControls(QWidget *parent) : QWidget(parent) {
    m_prevButton = makeButton(this, QStyle::SP_MediaSkipBackward, tr("Previous (P)"));
    m_playButton = makeButton(this, QStyle::SP_MediaPlay, tr("Play / Pause (Space)"));
    m_stopButton = makeButton(this, QStyle::SP_MediaStop, tr("Stop (S)"));
    m_nextButton = makeButton(this, QStyle::SP_MediaSkipForward, tr("Next (N)"));
    m_muteButton = makeButton(this, QStyle::SP_MediaVolume, tr("Mute (M)"));
    m_fullscreenButton = makeButton(this, QStyle::SP_TitleBarMaxButton, tr("Fullscreen (F)"));

    m_seekSlider = new QSlider(Qt::Horizontal, this);
    m_seekSlider->setRange(0, 0);
    m_seekSlider->setToolTip(tr("Seek"));

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setRange(0, 130);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(110);
    m_volumeSlider->setToolTip(tr("Volume"));

    m_timeLabel = new QLabel("00:00 / 00:00", this);
    m_timeLabel->setMinimumWidth(110);
    m_timeLabel->setAlignment(Qt::AlignCenter);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);
    layout->addWidget(m_prevButton);
    layout->addWidget(m_playButton);
    layout->addWidget(m_stopButton);
    layout->addWidget(m_nextButton);
    layout->addWidget(m_seekSlider, 1);
    layout->addWidget(m_timeLabel);
    layout->addWidget(m_muteButton);
    layout->addWidget(m_volumeSlider);
    layout->addWidget(m_fullscreenButton);

    connect(m_playButton, &QToolButton::clicked, this, &PlayerControls::playPauseClicked);
    connect(m_stopButton, &QToolButton::clicked, this, &PlayerControls::stopClicked);
    connect(m_prevButton, &QToolButton::clicked, this, &PlayerControls::previousClicked);
    connect(m_nextButton, &QToolButton::clicked, this, &PlayerControls::nextClicked);
    connect(m_muteButton, &QToolButton::clicked, this, &PlayerControls::muteClicked);
    connect(m_fullscreenButton, &QToolButton::clicked, this, &PlayerControls::fullscreenClicked);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &PlayerControls::volumeChanged);

    // Track when the user is actively scrubbing so position updates from the
    // player don't fight with the dragging handle.
    connect(m_seekSlider, &QSlider::sliderPressed, this, [this]() { m_seeking = true; });
    connect(m_seekSlider, &QSlider::sliderReleased, this, [this]() {
        m_seeking = false;
        emit seekRequested(m_seekSlider->value());
    });
    connect(m_seekSlider, &QSlider::sliderMoved, this, [this](int value) {
        m_position = value;
        refreshTimeLabel();
    });
}

QString PlayerControls::formatTime(double seconds) {
    if (seconds < 0 || !std::isfinite(seconds))
        seconds = 0;
    const qint64 total = static_cast<qint64>(seconds);
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;
    if (h > 0)
        return QString::asprintf("%lld:%02lld:%02lld", h, m, s);
    return QString::asprintf("%02lld:%02lld", m, s);
}

void PlayerControls::refreshTimeLabel() {
    m_timeLabel->setText(formatTime(m_position) + " / " + formatTime(m_duration));
}

void PlayerControls::setDuration(double seconds) {
    m_duration = seconds;
    m_seekSlider->setRange(0, static_cast<int>(seconds > 0 ? seconds : 0));
    refreshTimeLabel();
}

void PlayerControls::setPosition(double seconds) {
    m_position = seconds;
    if (!m_seeking)
        m_seekSlider->setValue(static_cast<int>(seconds));
    refreshTimeLabel();
}

void PlayerControls::setPlaying(bool playing) {
    m_playButton->setIcon(style()->standardIcon(
        playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
}

void PlayerControls::setVolume(int volume) {
    QSignalBlocker blocker(m_volumeSlider);
    m_volumeSlider->setValue(volume);
}

void PlayerControls::setMuted(bool muted) {
    m_muteButton->setIcon(style()->standardIcon(
        muted ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
}
