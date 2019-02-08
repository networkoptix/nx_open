#include "master_server_status_watcher.h"

#include <nx/utils/log/log.h>

#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

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

    connect(&m_takeMasterFlagTimer, &QTimer::timeout, this, [this]() { setMasterFlag(true); } );
    m_takeMasterFlagTimer.setSingleShot(true);
    m_takeMasterFlagTimer.setInterval(m_delayBeforeSettingMasterFlag.count());

    connect(
        &m_checkIfMasterFlagCanBeTakenTimer, &QTimer::timeout,
        this, [this]() { at_updateMasterFlag(); });
    m_checkIfMasterFlagCanBeTakenTimer.setInterval(m_delayBeforeSettingMasterFlag.count());
    m_checkIfMasterFlagCanBeTakenTimer.start();
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
    const auto betterMasterIter = std::find_if(items.begin(), items.end(),
        [this](const QnPeerRuntimeInfo& item)
        {
            return item.data.peer.id < m_commonModule->moduleGUID()
                && item.data.flags.testFlag(api::RuntimeFlag::masterCloudSync);
        });
    const auto hasBetterMaster = betterMasterIter != items.end();

    QnPeerRuntimeInfo localInfo = m_commonModule->runtimeInfoManager()->localInfo();
    bool isLocalMaster = localInfo.data.flags.testFlag(api::RuntimeFlag::masterCloudSync);
    bool canBeMaster = localPeerCanBeMaster() && !hasBetterMaster;

    NX_VERBOSE(this, lm("Checking local peer master flag state. "
        "isLocalMaster %1, canBeMaster %2, betterMasterPeerId %3")
        .args(isLocalMaster, canBeMaster,
            betterMasterIter != items.end() ? betterMasterIter->uuid.toSimpleByteArray() : ""));

    if (!canBeMaster && isLocalMaster)
    {
        m_takeMasterFlagTimer.stop();
        setMasterFlag(false);
    }
    else if (canBeMaster && !isLocalMaster)
    {
        if (!m_takeMasterFlagTimer.isActive())
            m_takeMasterFlagTimer.start(); //< Set master flag with delay.
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
