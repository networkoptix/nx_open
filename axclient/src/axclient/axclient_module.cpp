#include "axclient_module.h"

#include <api/app_server_connection.h>
#include <api/runtime_info_manager.h>

#include <client/client_module.h>
#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>

#include <nx_ec/ec_api.h>
#include <nx_ec/ec2_lib.h>

#include <plugins/resource/server_camera/server_camera_factory.h>

#include <ui/customization/customization.h>
#include <ui/customization/customizer.h>
#include <ui/style/skin.h>
#include <ui/style/globals.h>

#include <utils/network/module_finder.h>
#include <utils/network/router.h>
#include <utils/server_interface_watcher.h>

QnAxClientModule::QnAxClientModule(QObject *parent)
    : QObject(parent)
    , m_clientModule(new QnClientModule())
{
    qnSettings->setLightMode(Qn::LightModeActiveX);
    qnRuntime->setActiveXMode(true);

    QString customizationPath = qnSettings->clientSkin() == Qn::LightSkin 
        ? lit(":/skin_light") 
        : lit(":/skin_dark");
    m_skin.reset(new QnSkin(QStringList() << lit(":/skin") << customizationPath));

    QnCustomization customization;
    customization.add(QnCustomization(m_skin->path("customization_common.json")));
    customization.add(QnCustomization(m_skin->path("customization_base.json")));
    customization.add(QnCustomization(m_skin->path("customization_child.json")));

    m_customizer.reset(new QnCustomizer(customization));
    m_customizer->customize(qnGlobals);

    auto *style = QnSkin::newStyle();
    QApplication::setStyle(style);

    auto ec2ConnectionFactory = getConnectionFactory(Qn::PT_DesktopClient);
    ec2ConnectionFactory->setContext(ec2::ResourceContext(QnServerCameraFactory::instance(), qnResPool, qnResTypePool));
    QnAppServerConnectionFactory::setEC2ConnectionFactory(ec2ConnectionFactory);
    qnCommon->store<ec2::AbstractECConnectionFactory>(ec2ConnectionFactory);

    auto moduleFinder = new QnModuleFinder(true, qnRuntime->isDevMode());
    moduleFinder->start();
    qnCommon->store<QnModuleFinder>(moduleFinder);
    qnCommon->store<QnRouter>(new QnRouter(moduleFinder));
    qnCommon->store<QnServerInterfaceWatcher>(new QnServerInterfaceWatcher());

    //===========================================================================

    ec2::ApiRuntimeData runtimeData;
    runtimeData.peer.id = qnCommon->moduleGUID();
    runtimeData.peer.instanceId = qnCommon->runningInstanceGUID();
    runtimeData.peer.peerType = Qn::PT_DesktopClient;
    runtimeData.brand = QnAppInfo::productNameShort();
    QnRuntimeInfoManager::instance()->updateLocalItem(runtimeData);    // initializing localInfo
}

QnAxClientModule::~QnAxClientModule() {
    QnAppServerConnectionFactory::setEC2ConnectionFactory(nullptr);
}
