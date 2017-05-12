#include "client_core_module.h"

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client_core/client_core_settings.h>

#include <core/ptz/client_ptz_controller_pool.h>
#include <core/resource_management/resources_changes_manager.h>

#include <utils/media/ffmpeg_initializer.h>

#include <nx_ec/ec2_lib.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/timer_manager.h>

QnClientCoreModule::QnClientCoreModule(QObject* parent):
    base_type(parent)
{
    Q_INIT_RESOURCE(appserver2);

    m_commonModule = new QnCommonModule(true, this);

    commonModule()->store(new QnClientCoreSettings());
    commonModule()->store(new QnFfmpegInitializer());

    NX_ASSERT(nx::utils::TimerManager::instance());
    m_connectionFactory.reset(getConnectionFactory(qnStaticCommon->localPeerType(),
        nx::utils::TimerManager::instance(), commonModule()));

    commonModule()->instance<QnResourcesChangesManager>();
    commonModule()->instance<QnClientPtzControllerPool>();
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
