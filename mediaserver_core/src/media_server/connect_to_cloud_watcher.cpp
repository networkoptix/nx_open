#include "connect_to_cloud_watcher.h"

#include <nx/utils/std/cpp14.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <transaction/transaction_message_bus.h>
#include <cdb/ec2_request_paths.h>

namespace {
    static const int kUpdateIfFailIntervalMs = 1000 * 60;
}

using namespace nx::network::cloud;

QnConnectToCloudWatcher::QnConnectToCloudWatcher():
    m_cdbEndPointFetcher(
        new CloudDbUrlFetcher(std::make_unique<RandomEndpointSelector>()))
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

void QnConnectToCloudWatcher::restartTimer()
{
    m_timer.start();
}

void QnConnectToCloudWatcher::at_updateConnection()
{
    m_timer.stop();

    QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
    bool needCloudConnect =
        localInfo.data.flags.testFlag(ec2::RF_MasterCloudSync) &&
        !qnGlobalSettings->cloudSystemId().isEmpty() &&
		!qnGlobalSettings->cloudAuthKey().isEmpty();
    if (!needCloudConnect)
    {
        if (!m_cloudUrl.isEmpty())
            qnTransactionBus->removeConnectionFromPeer(m_cloudUrl);
        return;
    }

    m_cdbEndPointFetcher->get(
        nx_http::AuthInfo(),
        [this](int statusCode, QUrl url)
        {
            if (statusCode != nx_http::StatusCode::ok)
            {
                NX_LOGX(lm("Error fetching cloud_db endpoint. HTTP result: %1")
                    .str(statusCode), cl_logWARNING);
                // try once more later
                metaObject()->invokeMethod(this, "restartTimer", Qt::QueuedConnection);
                return;
            }

            NX_LOGX(lm("Creating transaction connection to cloud_db at %1")
                .str(url), cl_logDEBUG1);

            m_cloudUrl = url;
            m_cloudUrl.setPath(nx::cdb::api::kEc2EventsPath);
            m_cloudUrl.setUserName(qnGlobalSettings->cloudSystemId());
            m_cloudUrl.setPassword(qnGlobalSettings->cloudAuthKey());
            qnTransactionBus->addConnectionToPeer(m_cloudUrl);
        });
}
