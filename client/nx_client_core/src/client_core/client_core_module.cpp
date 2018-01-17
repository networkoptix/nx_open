#include "client_core_module.h"

#include <QtQml/QQmlEngine>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client_core/client_core_settings.h>

#include <nx/core/access/access_types.h>
#include <core/ptz/client_ptz_controller_pool.h>

#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/layout_tour_state_manager.h>

#include <utils/media/ffmpeg_initializer.h>

#include <nx_ec/ec2_lib.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/timer_manager.h>
#include <nx/client/core/watchers/known_server_connections.h>

using namespace nx::client::core;

QnClientCoreModule::QnClientCoreModule(QObject* parent):
    base_type(parent)
{
    Q_INIT_RESOURCE(appserver2);

    m_commonModule = new QnCommonModule(true, nx::core::access::Mode::cached, this);

    m_commonModule->store(new QnClientCoreSettings());
    m_commonModule->store(new QnFfmpegInitializer());

    NX_ASSERT(nx::utils::TimerManager::instance());
    m_connectionFactory.reset(getConnectionFactory(qnStaticCommon->localPeerType(),
        nx::utils::TimerManager::instance(), m_commonModule, false));

    m_commonModule->instance<QnResourcesChangesManager>();
    m_commonModule->instance<QnClientPtzControllerPool>();
    m_commonModule->instance<QnLayoutTourStateManager>();

    m_commonModule->store(new watchers::KnownServerConnections(m_commonModule));

    m_qmlEngine = new QQmlEngine(this);
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

QQmlEngine*QnClientCoreModule::mainQmlEngine()
{
    return m_qmlEngine;
}
