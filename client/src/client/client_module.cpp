#include "client_module.h"

#include <QtWidgets/QApplication>

#include <common/common_module.h>

#include "client_meta_types.h"

#include <utils/common/app_info.h>

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
	common->instance<QnClientSettings>();
    common->setModuleGUID(QnUuid::createUuid());
}

QnClientModule::~QnClientModule() {
    return;
}

