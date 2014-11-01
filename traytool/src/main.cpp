
#include "version.h"

#include <QtSingleApplication>
#include <QtWidgets/QMessageBox>
#include <QtCore/QMetaType>
#include <QtCore/QDir>
#include <QtCore/QSettings>

#include "systraywindow.h"
// #include "common/common_module.h"
// #include "translation/translation_manager.h"

static const QString CLIENT_NAME(QString(lit(QN_ORGANIZATION_NAME)) + lit(" ") + lit(QN_PRODUCT_NAME) + lit(" Client"));

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
    QApplication::setApplicationDisplayName(QLatin1String(QN_APPLICATION_DISPLAY_NAME));
    QApplication::setApplicationVersion(QLatin1String(QN_APPLICATION_VERSION));

    // Each user may have it's own traytool running.
    QtSingleApplication app(QLatin1String(QN_ORGANIZATION_NAME) + QLatin1String(QN_APPLICATION_NAME), argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

//    QnCommonModule common(argc, argv);

    QDir::setCurrent(QApplication::applicationDirPath());

//    QnTranslationManager *translationManager = qnCommon->instance<QnTranslationManager>();
//    translationManager->addPrefix(lit("traytool"));

    QSettings clientSettings(QSettings::UserScope, qApp->organizationName(), CLIENT_NAME);
    QString translationPath = clientSettings.value(lit("translationPath")).toString();
    int index = translationPath.lastIndexOf(lit("client"));
    if(index != -1)
        translationPath.replace(index, 6, lit("traytool"));
//    QnTranslation translation = translationManager->loadTranslation(translationPath);
//    QnTranslationManager::installTranslation(translation);

    QString argument = argc > 1 ? QLatin1String(argv[1]) : QString();
    if (argument == lit("quit"))
    {
        app.sendMessage(lit("quit"));

        // Wait for app to finish + 100ms just in case (in may be still running after unlocking QSingleApplication lock file).
        while (app.isRunning()) {
            Sleep(100);
        } 
        Sleep(100);

        return 0;
    }

    if (app.isRunning())
    {
        if (MyIsUserAnAdmin())
        {
            app.sendMessage(lit("quit"));

            // Need to wait while app quit
            // Otherwise QtSingleApplication behaves incorrectly
            while (app.isRunning())
                Sleep(100);
        }
        else {
            app.sendMessage(lit("activate"));
            return 0;
        }
    }

    QnSystrayWindow window;

    QObject::connect(&app, SIGNAL(messageReceived(const QString &)), &window, SLOT(handleMessage(const QString &)));

    if (!argument.isEmpty())
        window.executeAction(argument);

    return app.exec();
}
