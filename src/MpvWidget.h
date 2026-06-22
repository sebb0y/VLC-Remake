#pragma once

#include <QWidget>
#include <QStringList>
#include <mpv/client.h>

// MpvWidget embeds a libmpv player into a native Qt window.
//
// Rendering is delegated to mpv's own video output (the "wid" embedding
// strategy): we hand mpv the native window handle of this widget and it
// draws the decoded video directly into it. libmpv brings in FFmpeg, so
// every container/codec FFmpeg understands (MP4, MKV, AVI, WebM, MOV,
// FLV, ...) plays without extra work.
class MpvWidget : public QWidget {
    Q_OBJECT
public:
    explicit MpvWidget(QWidget *parent = nullptr);
    ~MpvWidget() override;

    // Fire-and-forget mpv command, e.g. {"loadfile", path} or {"cycle", "pause"}.
    void command(const QStringList &args);

    void setOptionString(const QString &name, const QString &value);
    void setPropertyString(const QString &name, const QString &value);
    double propertyDouble(const QString &name) const;
    bool propertyFlag(const QString &name) const;

    // High-level helpers used by the UI.
    void loadFile(const QString &path);
    void togglePause();
    void setPaused(bool paused);
    void stop();
    void seekAbsolute(double seconds);
    void seekRelative(double seconds);
    void setVolume(int volume);
    void toggleMute();

signals:
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void pauseChanged(bool paused);
    void volumeChanged(int volume);
    void muteChanged(bool muted);
    void mediaTitleChanged(const QString &title);
    void fileLoaded();
    void endFile();

private slots:
    void onMpvEvents();

private:
    void handleMpvEvent(mpv_event *event);
    static void wakeup(void *ctx);

    mpv_handle *m_mpv = nullptr;

signals:
    // Internal: emitted from mpv's wakeup callback, delivered to the GUI
    // thread via a queued connection so events are processed safely.
    void mpvEventsReady();
};
