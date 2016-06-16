#include <QtSingleApplication>
#include <QtWidgets/QMessageBox>
#include <QtCore/QMetaType>
#include <QtCore/QDir>
#include <QtCore/QSettings>

#include "systraywindow.h"
#include <traytool_app_info.h>

#include <utils/common/app_info.h>

#include <translation/translation_manager.h>

#include <nx/fusion/serialization/json_functions.h>

void initTranslations()
{
    static const QString clientName(QnAppInfo::organizationName() + lit(" ") + QnAppInfo::productName() + lit(" Client"));

    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(traytool);

    QScopedPointer<QnTranslationManager> translationManager(new QnTranslationManager());
    translationManager->addPrefix(lit("traytool"));

    QString defaultTranslation;

    /* Load from internal resource. */
    QFile file(lit(":/globals.json"));
    if(file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QJsonObject jsonObject;
        if(!QJson::deserialize(file.readAll(), &jsonObject))
        {
            NX_ASSERT(false, Q_FUNC_INFO, "Settings file could not be parsed!");
        }
        else
        {
            QJsonObject settingsObject = jsonObject.value(lit("settings")).toObject();
            defaultTranslation = settingsObject.value(lit("translationPath")).toString();
        }
    }

    QSettings clientSettings(QSettings::UserScope, qApp->organizationName(), clientName);
    QString translationPath = clientSettings.value(lit("translationPath"), defaultTranslation).toString();
    QnTranslation translation = translationManager->loadTranslation(translationPath);
    QnTranslationManager::installTranslation(translation);
}

int main(int argc, char *argv[])
{
    QApplication::setOrganizationName(QnAppInfo::organizationName());
    QApplication::setApplicationName(QnTraytoolAppInfo::applicationName());
    QApplication::setApplicationDisplayName(QnTraytoolAppInfo::applicationDisplayName());
    QApplication::setApplicationVersion(QnAppInfo::applicationVersion());

    // Each user may have it's own traytool running.
    QtSingleApplication app(QnAppInfo::organizationName() + QnTraytoolAppInfo::applicationName(), argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    QDir::setCurrent(QApplication::applicationDirPath());

    QString argument = argc > 1 ? QLatin1String(argv[1]) : QString();
    if (argument == lit("quit"))
    {
        app.sendMessage(lit("quit"));

        // Wait for app to finish + 100ms just in case (in may be still running after unlocking QSingleApplication lock file).
        while (app.isRunning())
        {
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

    initTranslations();

    if (!QSystemTrayIcon::isSystemTrayAvailable())
    {
        QMessageBox::critical(
            NULL,
            QObject::tr("System Tray"),
            QObject::tr("There is no system tray on this system.") + L'\n' + QObject::tr("Application will now quit.")
            );
        return 1;
    }

    QnSystrayWindow window;

    QObject::connect(&app, &QtSingleApplication::messageReceived, &window, &QnSystrayWindow::handleMessage);

    if (!argument.isEmpty())
        window.executeAction(argument);

    return app.exec();
}
