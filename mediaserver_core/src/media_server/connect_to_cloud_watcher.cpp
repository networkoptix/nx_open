#include "connect_to_cloud_watcher.h"

#include <common/common_module.h>
#include <transaction/transaction_message_bus.h>
#include <api/global_settings.h>

QnConnectToCloudWatcher::QnConnectToCloudWatcher()
{
    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoRemoved, this, &QnConnectToCloudWatcher::at_updateConnection);
}

void QnConnectToCloudWatcher::at_updateConnection()
{
    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    QUrl cloudUrl = qnGlobalSettings->cloudPortalUrl();
    bool needCloudConnect =
        localInfo.data.flags.testFlag(ec2::RF_MasterCloudSync) &&
        !cloudUrl.isEmpty() &&
        !qnGlobalSettings->cloudSystemID().isEmpty();

    if (!needCloudConnect)
    {
        if (!m_lastCloudUrl.isEmpty())
            qnTransactionBus->removeConnectionFromPeer(m_lastCloudUrl);
        m_lastCloudUrl.clear();
        return;
    }

    if (cloudUrl != m_lastCloudUrl)
    {
        if (!m_lastCloudUrl.isEmpty())
            qnTransactionBus->removeConnectionFromPeer(m_lastCloudUrl);
        qnTransactionBus->addConnectionToPeer(cloudUrl);
        m_lastCloudUrl = cloudUrl;
    }
}
