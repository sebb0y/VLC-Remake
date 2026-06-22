#include "MainWindow.h"

#include <clocale>

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Vela");
    QApplication::setApplicationDisplayName("Vela Media Player");
    QApplication::setOrganizationName("Vela");
    QApplication::setApplicationVersion("1.0.0");

    // libmpv requires the "C" numeric locale; Qt may have changed it.
    std::setlocale(LC_NUMERIC, "C");

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Vela — a VLC-inspired media player powered by libmpv/FFmpeg.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("files", "Video files or URLs to play.",
                                 "[files...]");
    parser.process(app);

    MainWindow window;
    window.resize(960, 600);
    window.show();

    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty())
        window.openPaths(files);

    return app.exec();
}
