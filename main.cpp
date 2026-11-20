#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QSettings>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Ralf Tralow"));
    app.setApplicationName(QStringLiteral("RacalDana1991"));
    app.setApplicationDisplayName(QStringLiteral("Racal-Dana 1991 / 1992"));
    app.setApplicationVersion(QStringLiteral(RACALDANA1991_VERSION));
    app.setWindowIcon(QIcon(QStringLiteral(":/device-icon.ico")));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QFont font = app.font();
    if (QFontDatabase::hasFamily(QStringLiteral("Inter"))) {
        font.setFamily(QStringLiteral("Inter"));
    }
    font.setPointSize(10);
    app.setFont(font);

    MainWindow window;
    QSettings settings;
    const auto geometryKey = QStringLiteral("window/geometry");
    window.restoreGeometry(settings.value(geometryKey).toByteArray());
    window.show();

    const int exitCode = app.exec();
    settings.setValue(geometryKey, window.saveGeometry());
    return exitCode;
}
