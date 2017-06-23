#include <QtSingleApplication>
#include <QtWidgets/QMessageBox>
#include <QtCore/QMetaType>
#include <QtCore/QDir>
#include <QtCore/QSettings>

#include "systraywindow.h"
#include <traytool_app_info.h>

#include <utils/common/app_info.h>

#include <translation/translation_manager.h>

QString translationPath30ToLocale(const QString& translationPath)
{
    const QString oldLocale = QnTranslationManager::translationPathToLocaleCode(translationPath);
    return QnTranslationManager::localeCode30to31(oldLocale);
}

QString readLocaleFromSettings(QSettings* settings)
{
    static const QString k30TranslationPath = lit("translationPath");
    static const QString kLocalePath = lit("locale");

    QString compatibleValue = settings->value(k30TranslationPath).toString();
    if (!compatibleValue.isEmpty())
        return translationPath30ToLocale(compatibleValue);
    return settings->value(kLocalePath, QnAppInfo::defaultLanguage()).toString();
}

void initTranslations()
{
    static const QString clientName(QnTraytoolAppInfo::clientName());

    Q_INIT_RESOURCE(common);
    Q_INIT_RESOURCE(traytool);

    QScopedPointer<QnTranslationManager> translationManager(new QnTranslationManager());
    translationManager->addPrefix(lit("traytool"));

    QSettings clientSettings(QSettings::UserScope, QnAppInfo::organizationName(), clientName);
    QString locale = readLocaleFromSettings(&clientSettings);
    QnTranslation translation = translationManager->loadTranslation(locale);
    QnTranslationManager::installTranslation(translation);
}

int main(int argc, char *argv[])
{
    QApplication::setOrganizationName(QnAppInfo::organizationName());
    QApplication::setApplicationName(QnTraytoolAppInfo::applicationName());
    QApplication::setApplicationVersion(QnAppInfo::applicationVersion());

    // Each user may have it's own traytool running.
    QtSingleApplication app(QnTraytoolAppInfo::applicationName(), argc, argv);
    QApplication::setWindowIcon(QIcon(lit(":/logo.png")));
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
