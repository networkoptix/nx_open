
#include "version.h"

#include <QtSingleApplication>
#include <QtGui>
#include <QMetaType>

#include <utils/network/networkoptixmodulefinder.h>
#include <utils/network/foundenterprisecontrollersmodel.h>

#include "systraywindow.h"
#include "common/common_module.h"
#include "translation/translation_manager.h"

static const QString CLIENT_NAME(QString(lit(QN_ORGANIZATION_NAME)) + lit(" ") + lit(QN_PRODUCT_NAME) + lit(" Client"));


int main(int argc, char *argv[])
{
    //RevealResponse response;
    //const char* str = 
    //    "{\n"
    //        "'version' : 1.3.1,\n"
    //        "'port' : 7001,\n"
    //        "'application' : \"enterprise ctrl\",\n"
    //        "'customization' : DW\n"
    //    "}";
    //const quint8* bufStart = (const quint8*)str;
    //const quint8* bufEnd = (const quint8*)str + strlen(str);
    //if( !response.deserialize( &bufStart, bufEnd ) )
    //    int x = 0;
    //return 0;



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

    QnCommonModule common(argc, argv);

    QDir::setCurrent(QApplication::applicationDirPath());


    QnTranslationManager *translationManager = qnCommon->instance<QnTranslationManager>();
    translationManager->addPrefix(lit("traytool"));

    QSettings clientSettings(QSettings::UserScope, qApp->organizationName(), CLIENT_NAME);
    QString translationPath = clientSettings.value(lit("translationPath")).toString();
    int index = translationPath.lastIndexOf(lit("client"));
    if(index != -1)
        translationPath.replace(index, 6, lit("traytool"));
    QnTranslation translation = translationManager->loadTranslation(translationPath);
    QnTranslationManager::installTranslation(translation);

    QString argument = argc > 1 ? lit(argv[1]) : QString();
    if (argument == lit("quit"))
    {
        app.sendMessage(lit("quit"));
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

    NetworkOptixModuleFinder nxModuleFinder;
    FoundEnterpriseControllersModel foundEnterpriseControllersModel( &nxModuleFinder );
    nxModuleFinder.start();

    QnSystrayWindow window(&foundEnterpriseControllersModel);

    QObject::connect(&app, SIGNAL(messageReceived(const QString&)), &window, SLOT(handleMessage(const QString&)));

    if (!argument.isEmpty())
        window.executeAction(argument);
    //window.show();
    return app.exec();
}
