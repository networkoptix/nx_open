#include "client_module.h"

#include <QtWidgets/QApplication>

#include <common/common_module.h>

#include "client_meta_types.h"

#include <utils/common/app_info.h>
#include <utils/common/command_line_parser.h>

#include "version.h"

QnClientModule::QnClientModule(int &argc, char **argv, QObject *parent): QObject(parent) {
    Q_INIT_RESOURCE(client);

    QnClientMetaTypes::initialize();

    /* Set up application parameters so that QSettings know where to look for settings. */
    QApplication::setOrganizationName(QnAppInfo::organizationName());
    QApplication::setApplicationName(lit(QN_APPLICATION_NAME));
    QApplication::setApplicationDisplayName(lit(QN_APPLICATION_DISPLAY_NAME));    
    if (QApplication::applicationVersion().isEmpty())
        QApplication::setApplicationVersion(QnAppInfo::applicationVersion());
 
    /* We don't want changes in desktop color settings to mess up our custom style. */
    QApplication::setDesktopSettingsAware(false);
 
    /* Init singletons. */
    QnCommonModule *common = new QnCommonModule(argc, argv, this);

    
    bool isLocalSettings = false;
    QnCommandLineParser commandLineParser;
    commandLineParser.addParameter(&isLocalSettings, "--local-settings",  NULL, QString());
    commandLineParser.parse(argc, argv, stderr, QnCommandLineParser::RemoveParsedParameters);

    QnClientSettings *settings = new QnClientSettings(isLocalSettings);
    common->store<QnClientSettings>(settings);

    common->setModuleGUID(QnUuid::createUuid());
}

QnClientModule::~QnClientModule() {
    QApplication::setOrganizationName(QString());
    QApplication::setApplicationName(QString());
    QApplication::setApplicationDisplayName(QString());
    QApplication::setApplicationVersion(QString());     
}

