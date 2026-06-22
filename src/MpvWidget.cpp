#include "MpvWidget.h"

#include <QByteArray>
#include <stdexcept>
#include <vector>

MpvWidget::MpvWidget(QWidget *parent) : QWidget(parent) {
    // The widget must own a real native window so mpv has something to
    // draw into. Creating it here makes winId() valid immediately.
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

    m_mpv = mpv_create();
    if (!m_mpv)
        throw std::runtime_error("Could not create the mpv player engine.");

    // Embed mpv into this widget's native window.
    auto wid = static_cast<int64_t>(winId());
    mpv_set_option(m_mpv, "wid", MPV_FORMAT_INT64, &wid);

    // We provide our own controls, so disable mpv's built-in OSC/keybindings.
    mpv_set_option_string(m_mpv, "osc", "no");
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-vo-keyboard", "no");
    mpv_set_option_string(m_mpv, "config", "no");
    mpv_set_option_string(m_mpv, "keep-open", "no");
    // Reasonable defaults for a desktop player.
    mpv_set_option_string(m_mpv, "hwdec", "auto-safe");
    mpv_set_option_string(m_mpv, "vo", "gpu");
    mpv_set_option_string(m_mpv, "ytdl", "yes");

    if (mpv_initialize(m_mpv) < 0)
        throw std::runtime_error("Could not initialise the mpv player engine.");

    // Ask mpv to notify us whenever these properties change.
    mpv_observe_property(m_mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 0, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 0, "media-title", MPV_FORMAT_STRING);

    connect(this, &MpvWidget::mpvEventsReady, this, &MpvWidget::onMpvEvents,
            Qt::QueuedConnection);
    mpv_set_wakeup_callback(m_mpv, MpvWidget::wakeup, this);
}

MpvWidget::~MpvWidget() {
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void MpvWidget::wakeup(void *ctx) {
    // Called on an arbitrary mpv thread. Just hop to the GUI thread.
    auto *self = static_cast<MpvWidget *>(ctx);
    emit self->mpvEventsReady();
}

void MpvWidget::onMpvEvents() {
    if (!m_mpv)
        return;
    while (true) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
        handleMpvEvent(event);
    }
}

void MpvWidget::handleMpvEvent(mpv_event *event) {
    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        auto *prop = static_cast<mpv_event_property *>(event->data);
        const QString name = QString::fromUtf8(prop->name);
        if (name == "time-pos" && prop->format == MPV_FORMAT_DOUBLE) {
            emit positionChanged(*static_cast<double *>(prop->data));
        } else if (name == "duration" && prop->format == MPV_FORMAT_DOUBLE) {
            emit durationChanged(*static_cast<double *>(prop->data));
        } else if (name == "pause" && prop->format == MPV_FORMAT_FLAG) {
            emit pauseChanged(*static_cast<int *>(prop->data) != 0);
        } else if (name == "mute" && prop->format == MPV_FORMAT_FLAG) {
            emit muteChanged(*static_cast<int *>(prop->data) != 0);
        } else if (name == "volume" && prop->format == MPV_FORMAT_DOUBLE) {
            emit volumeChanged(static_cast<int>(*static_cast<double *>(prop->data)));
        } else if (name == "media-title" && prop->format == MPV_FORMAT_STRING) {
            emit mediaTitleChanged(QString::fromUtf8(*static_cast<char **>(prop->data)));
        }
        break;
    }
    case MPV_EVENT_FILE_LOADED:
        emit fileLoaded();
        break;
    case MPV_EVENT_END_FILE:
        emit endFile();
        break;
    default:
        break;
    }
}

void MpvWidget::command(const QStringList &args) {
    if (!m_mpv)
        return;
    std::vector<QByteArray> owned;
    owned.reserve(args.size());
    for (const QString &arg : args)
        owned.push_back(arg.toUtf8());

    std::vector<const char *> argv;
    argv.reserve(owned.size() + 1);
    for (const QByteArray &arg : owned)
        argv.push_back(arg.constData());
    argv.push_back(nullptr);

    // mpv copies the argument array, so the locals can go out of scope.
    mpv_command_async(m_mpv, 0, argv.data());
}

void MpvWidget::setOptionString(const QString &name, const QString &value) {
    if (m_mpv)
        mpv_set_option_string(m_mpv, name.toUtf8().constData(), value.toUtf8().constData());
}

void MpvWidget::setPropertyString(const QString &name, const QString &value) {
    if (m_mpv)
        mpv_set_property_string(m_mpv, name.toUtf8().constData(), value.toUtf8().constData());
}

double MpvWidget::propertyDouble(const QString &name) const {
    double value = 0.0;
    if (m_mpv)
        mpv_get_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_DOUBLE, &value);
    return value;
}

bool MpvWidget::propertyFlag(const QString &name) const {
    int value = 0;
    if (m_mpv)
        mpv_get_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_FLAG, &value);
    return value != 0;
}

void MpvWidget::loadFile(const QString &path) {
    command({"loadfile", path});
}

void MpvWidget::togglePause() {
    command({"cycle", "pause"});
}

void MpvWidget::setPaused(bool paused) {
    setPropertyString("pause", paused ? "yes" : "no");
}

void MpvWidget::stop() {
    command({"stop"});
}

void MpvWidget::seekAbsolute(double seconds) {
    command({"seek", QString::number(seconds), "absolute"});
}

void MpvWidget::seekRelative(double seconds) {
    command({"seek", QString::number(seconds), "relative"});
}

void MpvWidget::setVolume(int volume) {
    setPropertyString("volume", QString::number(volume));
}

void MpvWidget::toggleMute() {
    command({"cycle", "mute"});
}
