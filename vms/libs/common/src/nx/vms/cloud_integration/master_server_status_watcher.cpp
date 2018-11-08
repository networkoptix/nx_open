#include "master_server_status_watcher.h"

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/log/log_main.h>

namespace nx {
namespace vms {
namespace cloud_integration {

QnMasterServerStatusWatcher::QnMasterServerStatusWatcher(
    QnCommonModule* commonModule,
    std::chrono::milliseconds delayBeforeSettingMasterFlag)
    :
    m_commonModule(commonModule),
    m_delayBeforeSettingMasterFlag(delayBeforeSettingMasterFlag)
{
    connect(
        m_commonModule->resourcePool(), &QnResourcePool::resourceAdded,
        this,
        [this](const QnResourcePtr& resource)
        {
            auto server = resource.dynamicCast<QnMediaServerResource>();
            if (server && resource->getId() == m_commonModule->moduleGUID())
            {
                connect(
                    server.data(), &QnMediaServerResource::serverFlagsChanged,
                    this, &QnMasterServerStatusWatcher::at_updateMasterFlag);
                at_updateMasterFlag();
            }
        });

    connect(
        m_commonModule->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoAdded,
        this, &QnMasterServerStatusWatcher::at_updateMasterFlag, Qt::QueuedConnection);
    connect(
        m_commonModule->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged,
        this, &QnMasterServerStatusWatcher::at_updateMasterFlag, Qt::QueuedConnection);
    connect(
        m_commonModule->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoRemoved,
        this, &QnMasterServerStatusWatcher::at_updateMasterFlag, Qt::QueuedConnection);

    connect(&m_timer, &QTimer::timeout, this, [this]() { setMasterFlag(true); } );
    m_timer.setSingleShot(true);
    m_timer.setInterval(m_delayBeforeSettingMasterFlag.count());
}

bool QnMasterServerStatusWatcher::localPeerCanBeMaster() const
{
    auto mServer = m_commonModule->resourcePool()->getResourceById<QnMediaServerResource>(
        m_commonModule->moduleGUID());
    return mServer && mServer->getServerFlags().testFlag(vms::api::SF_HasPublicIP);
}

void QnMasterServerStatusWatcher::at_updateMasterFlag()
{
    auto items = m_commonModule->runtimeInfoManager()->items()->getItems();
    bool hasBetterMaster = std::any_of(items.begin(), items.end(),
        [this](const QnPeerRuntimeInfo& item)
        {
            return item.data.peer.id < m_commonModule->moduleGUID()
                && item.data.flags.testFlag(api::RuntimeFlag::masterCloudSync);
        });

    QnPeerRuntimeInfo localInfo = m_commonModule->runtimeInfoManager()->localInfo();
    bool isLocalMaster = localInfo.data.flags.testFlag(api::RuntimeFlag::masterCloudSync);
    bool canBeMaster = localPeerCanBeMaster() && !hasBetterMaster;
    if (!canBeMaster && isLocalMaster)
    {
        m_timer.stop();
        setMasterFlag(false);
    }
    else if (canBeMaster && !isLocalMaster)
    {
        if (!m_timer.isActive())
            m_timer.start(); //< Set master flag with delay.
    }
}

void QnMasterServerStatusWatcher::setMasterFlag(bool value)
{
    NX_DEBUG(this, "Set master flag to value %1 for server %2", value, m_commonModule->moduleGUID());

    QnPeerRuntimeInfo localInfo = m_commonModule->runtimeInfoManager()->localInfo();
    if (value)
        localInfo.data.flags |= api::RuntimeFlag::masterCloudSync;
    else
        localInfo.data.flags &= ~api::RuntimeFlags(api::RuntimeFlag::masterCloudSync);

    m_commonModule->runtimeInfoManager()->updateLocalItem(localInfo);
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
