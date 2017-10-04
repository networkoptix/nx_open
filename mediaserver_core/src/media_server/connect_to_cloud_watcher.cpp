#include "connect_to_cloud_watcher.h"

#include <nx/utils/std/cpp14.h>

#include <api/global_settings.h>
#include <common/common_module.h>
#include <transaction/message_bus_adapter.h>
#include <transaction/transaction_transport_base.h>
#include <nx/cloud/cdb/api/ec2_request_paths.h>

#include "settings.h"
#include "media_server_module.h"
#include <api/runtime_info_manager.h>

namespace {
    static const int kUpdateIfFailIntervalMs = 1000 * 60;
}

using namespace nx::network::cloud;

QnConnectToCloudWatcher::QnConnectToCloudWatcher(ec2::AbstractTransactionMessageBus* messageBus):
    m_cdbEndPointFetcher(new CloudDbUrlFetcher()),
    m_messageBus(messageBus)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(kUpdateIfFailIntervalMs);

    const auto& commonModule = messageBus->commonModule();
    connect(&m_timer, &QTimer::timeout, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(commonModule->globalSettings(), &QnGlobalSettings::cloudSettingsChanged, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(commonModule->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoAdded, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(commonModule->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(commonModule->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoRemoved, this, &QnConnectToCloudWatcher::at_updateConnection);
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
    const auto& commonModule = m_messageBus->commonModule();
    QnPeerRuntimeInfo localInfo = commonModule->runtimeInfoManager()->localInfo();
    bool needCloudConnect =
        localInfo.data.flags.testFlag(ec2::RF_MasterCloudSync) &&
        !commonModule->globalSettings()->cloudSystemId().isEmpty() &&
		!commonModule->globalSettings()->cloudAuthKey().isEmpty();
    if (!needCloudConnect)
    {
        if (!m_cloudUrl.isEmpty())
            m_messageBus->removeOutgoingConnectionFromPeer(::ec2::kCloudPeerId);
        return;
    }

    const auto cdbEndpoint =
        qnServerModule->roSettings()->value(nx_ms_conf::CDB_ENDPOINT, "").toString();
    if (!cdbEndpoint.isEmpty())
    {
        addCloudPeer((QString)lm("http://%1").arg(cdbEndpoint));
    }
    else
    {
        m_cdbEndPointFetcher->get(
            nx_http::AuthInfo(),
            [this](int statusCode, nx::utils::Url url)
            {
                if (statusCode != nx_http::StatusCode::ok)
                {
                    NX_LOGX(lm("Error fetching cloud_db endpoint. HTTP result: %1")
                        .arg(statusCode), cl_logWARNING);
                    // try once more later
                    metaObject()->invokeMethod(this, "restartTimer", Qt::QueuedConnection);
                    return;
                }

                addCloudPeer(url);
            });
    }
}

void QnConnectToCloudWatcher::addCloudPeer(nx::utils::Url url)
{
    const auto& commonModule = m_messageBus->commonModule();
    NX_LOGX(lm("Creating transaction connection to cloud_db at %1")
        .arg(url), cl_logDEBUG1);

    m_cloudUrl = url;
    m_cloudUrl.setPath(nx::cdb::api::kEc2EventsPath);
    m_cloudUrl.setUserName(commonModule->globalSettings()->cloudSystemId());
    m_cloudUrl.setPassword(commonModule->globalSettings()->cloudAuthKey());
    m_messageBus->addOutgoingConnectionToPeer(::ec2::kCloudPeerId, m_cloudUrl);
}
