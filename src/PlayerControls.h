#pragma once

#include <QWidget>

class QSlider;
class QToolButton;
class QLabel;

// PlayerControls is the bottom transport bar: play/pause, stop, previous/
// next, a seek slider with time readouts, a volume slider, and fullscreen.
// It is intentionally "dumb" — it emits intent signals and exposes setters,
// while MainWindow wires it to the actual player.
class PlayerControls : public QWidget {
    Q_OBJECT
public:
    explicit PlayerControls(QWidget *parent = nullptr);

public slots:
    void setDuration(double seconds);
    void setPosition(double seconds);
    void setPlaying(bool playing);
    void setVolume(int volume);
    void setMuted(bool muted);

signals:
    void playPauseClicked();
    void stopClicked();
    void previousClicked();
    void nextClicked();
    void seekRequested(double seconds);
    void volumeChanged(int volume);
    void muteClicked();
    void fullscreenClicked();

private:
    static QString formatTime(double seconds);
    void refreshTimeLabel();

    QToolButton *m_playButton = nullptr;
    QToolButton *m_stopButton = nullptr;
    QToolButton *m_prevButton = nullptr;
    QToolButton *m_nextButton = nullptr;
    QToolButton *m_muteButton = nullptr;
    QToolButton *m_fullscreenButton = nullptr;
    QSlider *m_seekSlider = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QLabel *m_timeLabel = nullptr;

    double m_duration = 0.0;
    double m_position = 0.0;
    bool m_seeking = false;
};
