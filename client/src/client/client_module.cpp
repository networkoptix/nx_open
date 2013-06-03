#include "client_module.h"

#include <utils/common/module_resources.h>

#include <common/common_module.h>

#include "client_meta_types.h"
#include "client_settings.h"

#include "version.h"

QnClientModule::QnClientModule(int &argc, char **argv, QObject *parent): QObject(parent) {
    QN_INIT_MODULE_RESOURCES(client);

    QnClientMetaTypes::initialize();

    /* Set up application parameters so that QSettings know where to look for settings. */
    QApplication::setOrganizationName(QLatin1String(QN_ORGANIZATION_NAME));
    QApplication::setApplicationName(QLatin1String(QN_APPLICATION_NAME));
    QApplication::setApplicationVersion(QLatin1String(QN_APPLICATION_VERSION));

    /* We don't want changes in desktop color settings to mess up our custom style. */
    QApplication::setDesktopSettingsAware(false);

    /* Init singletons. */
    new QnCommonModule(argc, argv, this);
    new QnClientSettings(this);
}

QnClientModule::~QnClientModule() {
    return;
}

