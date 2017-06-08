#include "master_server_status_watcher.h"
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

QnMasterServerStatusWatcher::QnMasterServerStatusWatcher(
    QObject* parent,
    std::chrono::milliseconds delayBeforeSettingMasterFlag)
    :
    QObject(parent),
    QnCommonModuleAware(parent),
    m_delayBeforeSettingMasterFlag(delayBeforeSettingMasterFlag)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            auto server = resource.dynamicCast<QnMediaServerResource>();
            if (server && resource->getId() == commonModule()->moduleGUID())
            {
                connect(server.data(), &QnMediaServerResource::serverFlagsChanged, this, &QnMasterServerStatusWatcher::at_updateMasterFlag);
                at_updateMasterFlag();
            }
        });

    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnMasterServerStatusWatcher::at_updateMasterFlag, Qt::QueuedConnection);
    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &QnMasterServerStatusWatcher::at_updateMasterFlag, Qt::QueuedConnection);
    connect(runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoRemoved, this, &QnMasterServerStatusWatcher::at_updateMasterFlag, Qt::QueuedConnection);
    connect(&m_timer, &QTimer::timeout, this, [this]() { setMasterFlag(true); } );
    m_timer.setSingleShot(true);
    m_timer.setInterval(m_delayBeforeSettingMasterFlag.count());
}

bool QnMasterServerStatusWatcher::localPeerCanBeMaster() const
{
    auto mServer = resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->moduleGUID());
    return mServer && mServer->getServerFlags().testFlag(Qn::SF_HasPublicIP);
}

void QnMasterServerStatusWatcher::at_updateMasterFlag()
{
    auto items = runtimeInfoManager()->items()->getItems();
    bool hasBetterMaster = std::any_of(items.begin(), items.end(),
        [this](const QnPeerRuntimeInfo& item)
    {
        return item.data.peer.id < commonModule()->moduleGUID() &&
               item.data.flags.testFlag(ec2::RF_MasterCloudSync);
    });

    QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
    bool isLocalMaster = localInfo.data.flags.testFlag(ec2::RF_MasterCloudSync);
    bool canBeMaster = localPeerCanBeMaster() && !hasBetterMaster;
    if (!canBeMaster && isLocalMaster)
    {
        m_timer.stop();
        setMasterFlag(false);
    }
    else if (canBeMaster && !isLocalMaster)
    {
        if (!m_timer.isActive())
            m_timer.start(); //< set master flag with delay
    }
}

void QnMasterServerStatusWatcher::setMasterFlag(bool value)
{
    QnPeerRuntimeInfo localInfo = runtimeInfoManager()->localInfo();
    if (value)
        localInfo.data.flags |= ec2::RF_MasterCloudSync;
    else
        localInfo.data.flags &= ~ec2::RF_MasterCloudSync;
    runtimeInfoManager()->updateLocalItem(localInfo);
}
