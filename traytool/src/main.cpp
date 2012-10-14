#include "version.h"

#include <QtSingleApplication>
#include <QtGui>

#include "systraywindow.h"

int main(int argc, char *argv[])
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::critical(
            NULL, 
            QObject::tr("System Tray"), 
            QObject::tr("There is no system tray on this system.\nApplication will now quit.")
        );
        return 1;
    }

    QApplication::setOrganizationName(QLatin1String(QN_ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(QN_APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(QN_APPLICATION_VERSION));

    // Each user may have it's own traytool running.
    QtSingleApplication app(QLatin1String(QN_ORGANIZATION_NAME) + QLatin1String(QN_APPLICATION_NAME), argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    QDir::setCurrent(QFileInfo(QFile::decodeName(argv[0])).absolutePath());

    if (argc > 1 && QString(argv[1]) == "quit")
    {
        app.sendMessage("quit");
        return 0;
    }

    if (app.isRunning())
    {
        if (MyIsUserAnAdmin())
        {
            app.sendMessage("quit");

            // Need to wait while app quit
            // Otherwise QtSingleApplication behaves incorrectly
            while (app.isRunning())
                Sleep(100);
        }
        else {
            app.sendMessage("activate");
            return 0;
        }
    }

    QnSystrayWindow window;

    QObject::connect(&app, SIGNAL(messageReceived(const QString&)),
        &window, SLOT(handleMessage(const QString&)));

    if (argc > 1)
        window.executeAction(argv[1]);
    //window.show();
    return app.exec();
}
