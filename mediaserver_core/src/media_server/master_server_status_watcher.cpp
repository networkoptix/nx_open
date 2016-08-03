#include "master_server_status_watcher.h"
#include <common/common_module.h>

namespace {
    static const int kUpdateMasterFlagTimeoutMs = 1000 * 30;
}

MasterServerStatusWatcher::MasterServerStatusWatcher()
{
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &MasterServerStatusWatcher::at_runtimeInfoChanged);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &MasterServerStatusWatcher::at_runtimeInfoChanged);
    connect(&m_timer, &QTimer::timeout, this, [this]() { setMasterFlag(true); } );
    m_timer.setSingleShot(true);
    m_timer.setInterval(kUpdateMasterFlagTimeoutMs);
    m_timer.start();
}

void MasterServerStatusWatcher::at_runtimeInfoChanged(const QnPeerRuntimeInfo& runtimeInfo)
{
    if (runtimeInfo.uuid == qnCommon->moduleGUID())
        return;

    auto items = QnRuntimeInfoManager::instance()->items()->getItems();
    bool hasBetterMaster = std::any_of(items.begin(), items.end(),
        [this](const QnPeerRuntimeInfo& item)
    {
        return item.data.peer.id < qnCommon->moduleGUID() &&
               item.data.flags.testFlag(ec2::RF_MasterCloudSync);
    });

    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    bool isLocalMaster = localInfo.data.flags.testFlag(ec2::RF_MasterCloudSync);
    if (hasBetterMaster && isLocalMaster)
    {
        m_timer.stop();
        setMasterFlag(false);
    }
    else if (!hasBetterMaster && !isLocalMaster)
    {
        if (!m_timer.isActive())
            m_timer.start(); //< set master flag with delay
    }
}

void MasterServerStatusWatcher::setMasterFlag(bool value)
{
    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    if (value)
        localInfo.data.flags |= ec2::RF_MasterCloudSync;
    else
        localInfo.data.flags &= ~ec2::RF_MasterCloudSync;
    QnRuntimeInfoManager::instance()->updateLocalItem(localInfo);
}
