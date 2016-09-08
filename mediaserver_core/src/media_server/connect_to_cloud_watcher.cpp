#include "connect_to_cloud_watcher.h"

#include <nx/utils/std/cpp14.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <transaction/transaction_message_bus.h>

namespace {
    static const int kUpdateIfFailIntervalMs = 1000 * 60;
}

using namespace nx::network::cloud;

QnConnectToCloudWatcher::QnConnectToCloudWatcher():
    m_cdbEndPointFetcher(new CloudModuleEndPointFetcher(
        "cdb",
        std::make_unique<RandomEndpointSelector>()))
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(kUpdateIfFailIntervalMs);

    connect(&m_timer, &QTimer::timeout, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(qnGlobalSettings, &QnGlobalSettings::cloudSettingsChanged, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(QnRuntimeInfoManager::instance(), &QnRuntimeInfoManager::runtimeInfoRemoved, this, &QnConnectToCloudWatcher::at_updateConnection);
}

QnConnectToCloudWatcher::~QnConnectToCloudWatcher()
{
    m_cdbEndPointFetcher->pleaseStopSync();
}

void QnConnectToCloudWatcher::at_updateConnection()
{
    m_timer.stop();

    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    bool needCloudConnect =
        localInfo.data.flags.testFlag(ec2::RF_MasterCloudSync) &&
        !qnGlobalSettings->cloudSystemID().isEmpty();
    if (!needCloudConnect)
    {
        if (!m_cloudUrl.isEmpty())
            qnTransactionBus->removeConnectionFromPeer(m_cloudUrl);
        return;
    }

    m_cdbEndPointFetcher->get(
        nx_http::AuthInfo(),
        [this](int statusCode, boost::optional<SocketAddress> endpoint)
        {
            if (statusCode != nx_http::StatusCode::ok || endpoint->isNull())
            {
                m_timer.start(); //< try once more later
                return;
            }

            m_cloudUrl = QUrl(lit("http://%1:%2/ec2/events").
                arg(endpoint->address.toString()).
                arg(endpoint->port));
            m_cloudUrl.setUserName(qnGlobalSettings->cloudSystemID());
            m_cloudUrl.setPassword(qnGlobalSettings->cloudAuthKey());
            qnTransactionBus->addConnectionToPeer(m_cloudUrl);
        });
}
