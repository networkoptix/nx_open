#include "client_core_module.h"

#include <QtQml/QQmlEngine>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client_core/client_core_meta_types.h>
#include <client_core/client_core_settings.h>

#include <nx/core/access/access_types.h>
#include <core/ptz/client_ptz_controller_pool.h>

#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/layout_tour_state_manager.h>
#include <core/dataprovider/data_provider_factory.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/timer_manager.h>
#include <nx/client/core/watchers/known_server_connections.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/client/core/settings/migration.h>
#include <ec2/remote_connection_factory.h>
#include <nx/client/core/utils/operation_manager.h>
#include <plugins/resource/desktop_audio_only/desktop_audio_only_resource.h>

#include <nx/utils/app_info.h>

using namespace nx::vms::client::core;

QnClientCoreModule::QnClientCoreModule(QObject* parent):
    base_type(parent)
{
    m_commonModule = new QnCommonModule(true, nx::core::access::Mode::cached, this);

    Q_INIT_RESOURCE(nx_vms_client_core);
    initializeMetaTypes();

    m_commonModule->store(new QnClientCoreSettings());

    using nx::utils::AppInfo;

    m_commonModule->store(new Settings());
    settings_migration::migrate();

    NX_ASSERT(nx::utils::TimerManager::instance());
    m_connectionFactory.reset(new ec2::RemoteConnectionFactory(
        m_commonModule,
        qnStaticCommon->localPeerType(),
        false));

    m_commonModule->instance<QnResourcesChangesManager>();
    m_commonModule->instance<QnClientPtzControllerPool>();
    m_commonModule->instance<QnLayoutTourStateManager>();

    m_commonModule->store(new watchers::KnownServerConnections(m_commonModule));
    m_commonModule->store(new OperationManager());

    m_resourceDataProviderFactory.reset(new QnDataProviderFactory());

    m_qmlEngine = new QQmlEngine(this);
    m_qmlEngine->setOutputWarningsToStandardError(true);

    registerResourceDataProviders();
}

QnClientCoreModule::~QnClientCoreModule()
{

}

QnCommonModule* QnClientCoreModule::commonModule() const
{
    return m_commonModule;
}

ec2::AbstractECConnectionFactory* QnClientCoreModule::connectionFactory() const
{
    return m_connectionFactory.get();
}

QnPtzControllerPool* QnClientCoreModule::ptzControllerPool() const
{
    return m_commonModule->instance<QnClientPtzControllerPool>();
}

QnLayoutTourStateManager* QnClientCoreModule::layoutTourStateManager() const
{
    return m_commonModule->instance<QnLayoutTourStateManager>();
}

QnDataProviderFactory* QnClientCoreModule::dataProviderFactory() const
{
    return m_resourceDataProviderFactory.data();
}

QQmlEngine*QnClientCoreModule::mainQmlEngine()
{
    return m_qmlEngine;
}

void QnClientCoreModule::registerResourceDataProviders()
{
    m_resourceDataProviderFactory->registerResourceType<QnDesktopAudioOnlyResource>();
}
