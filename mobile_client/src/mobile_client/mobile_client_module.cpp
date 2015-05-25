#include "mobile_client_module.h"

#include <QtGui/QGuiApplication>

#include "common/common_module.h"
#include "mobile_client/mobile_client_meta_types.h"
#include "mobile_client/mobile_client_settings.h"

#include "version.h"

QnMobileClientModule::QnMobileClientModule(int &argc, char *argv[], QObject *parent) :
    QObject(parent)
{
    Q_INIT_RESOURCE(mobile_client);
    QnMobileClientMetaTypes::initialize();

    /* Set up application parameters so that QSettings know where to look for settings. */
    QGuiApplication::setOrganizationName(lit(QN_ORGANIZATION_NAME));
    QGuiApplication::setApplicationName(lit(QN_APPLICATION_NAME));
    QGuiApplication::setApplicationVersion(lit(QN_APPLICATION_VERSION));

    /* Init singletons. */
    QnCommonModule *common = new QnCommonModule(argc, argv, this);
    common->instance<QnMobileClientSettings>();
    common->setModuleGUID(QnUuid::createUuid());
}
