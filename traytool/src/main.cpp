#include "version.h"

#include <QtSingleApplication>
#include <QtGui>

#include "systraywindow.h"

int main(int argc, char *argv[])
{
    Q_INIT_RESOURCE(traytool);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(0, QObject::tr("Systray"),
            QObject::tr("I couldn't detect any system tray "
            "on this system."));
        return 1;
    }

    QApplication::setOrganizationName(QLatin1String(ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(APPLICATION_VERSION));

    QtSingleApplication* singleApplication = new QtSingleApplication(argc, argv);
    singleApplication->setQuitOnLastWindowClosed(false);

    //singleApplication->setWindowIcon(Skin::icon(QLatin1String("appicon.png")));

    if (singleApplication->isRunning()) {
        return 0;
    }

    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());

    QnSystrayWindow window;
    //window.show();
    return singleApplication->exec();
}
