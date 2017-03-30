#include "master_server_status_watcher.h"
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

namespace {
    static const int kUpdateMasterFlagTimeoutMs = 1000 * 30;
}

QnMasterServerStatusWatcher::QnMasterServerStatusWatcher()
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            auto server = resource.dynamicCast<QnMediaServerResource>();
            if (server && resource->getId() == qnCommon->moduleGUID())
            {
                connect(server.data(), &QnMediaServerResource::serverFlagsChanged, this, &QnMasterServerStatusWatcher::at_updateMasterFlag);
                at_updateMasterFlag();
            }
        });

    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnMasterServerStatusWatcher::at_updateMasterFlag, Qt::QueuedConnection);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &QnMasterServerStatusWatcher::at_updateMasterFlag, Qt::QueuedConnection);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoRemoved, this, &QnMasterServerStatusWatcher::at_updateMasterFlag, Qt::QueuedConnection);
    connect(&m_timer, &QTimer::timeout, this, [this]() { setMasterFlag(true); } );
    m_timer.setSingleShot(true);
    m_timer.setInterval(kUpdateMasterFlagTimeoutMs);
}

bool QnMasterServerStatusWatcher::localPeerCanBeMaster() const
{
    auto mServer = resourcePool()->getResourceById<QnMediaServerResource>(qnCommon->moduleGUID());
    return mServer && mServer->getServerFlags().testFlag(Qn::SF_HasPublicIP);
}

void QnMasterServerStatusWatcher::at_updateMasterFlag()
{
    auto items = QnRuntimeInfoManager::instance()->items()->getItems();
    bool hasBetterMaster = std::any_of(items.begin(), items.end(),
        [this](const QnPeerRuntimeInfo& item)
    {
        return item.data.peer.id < qnCommon->moduleGUID() &&
               item.data.flags.testFlag(ec2::RF_MasterCloudSync);
    });

    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
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
    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    if (value)
        localInfo.data.flags |= ec2::RF_MasterCloudSync;
    else
        localInfo.data.flags &= ~ec2::RF_MasterCloudSync;
    QnRuntimeInfoManager::instance()->updateLocalItem(localInfo);
}
