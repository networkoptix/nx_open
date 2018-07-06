#include "connect_to_cloud_watcher.h"

#include <nx/utils/std/cpp14.h>

#include <nx/cloud/cdb/api/ec2_request_paths.h>

#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <common/common_module.h>

namespace { static const int kUpdateIfFailIntervalMs = 1000 * 60; }

using namespace nx::network::cloud;

namespace nx {
namespace vms {
namespace cloud_integration {

QnConnectToCloudWatcher::QnConnectToCloudWatcher(
    QnCommonModule* commonModule,
    AbstractEc2CloudConnector* ec2CloudConnector)
    :
    m_commonModule(commonModule),
    m_ec2CloudConnector(ec2CloudConnector),
    m_cdbEndPointFetcher(new CloudDbUrlFetcher())
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(kUpdateIfFailIntervalMs);

    connect(&m_timer, &QTimer::timeout, this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(
        m_commonModule->globalSettings(), &QnGlobalSettings::cloudSettingsChanged,
        this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(
        m_commonModule->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoAdded,
        this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(
        m_commonModule->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoChanged,
        this, &QnConnectToCloudWatcher::at_updateConnection);
    connect(
        m_commonModule->runtimeInfoManager(), &QnRuntimeInfoManager::runtimeInfoRemoved,
        this, &QnConnectToCloudWatcher::at_updateConnection);
}

QnConnectToCloudWatcher::~QnConnectToCloudWatcher()
{
    m_cdbEndPointFetcher->pleaseStopSync();
}

void QnConnectToCloudWatcher::setCloudDbUrl(const nx::utils::Url& cloudDbUrl)
{
    m_cloudDbUrl = cloudDbUrl;
}

void QnConnectToCloudWatcher::restartTimer()
{
    m_timer.start();
}

void QnConnectToCloudWatcher::at_updateConnection()
{
    m_timer.stop();
    QnPeerRuntimeInfo localInfo = m_commonModule->runtimeInfoManager()->localInfo();
    bool needCloudConnect =
        localInfo.data.flags.testFlag(ec2::RF_MasterCloudSync) &&
        !m_commonModule->globalSettings()->cloudSystemId().isEmpty() &&
		!m_commonModule->globalSettings()->cloudAuthKey().isEmpty();
    if (!needCloudConnect)
    {
        if (!m_cloudUrl.isEmpty())
            m_ec2CloudConnector->stopDataSynchronization();
        return;
    }

    if (m_cloudDbUrl)
    {
        addCloudPeer(*m_cloudDbUrl);
    }
    else
    {
        m_cdbEndPointFetcher->get(
            nx::network::http::AuthInfo(),
            [this](int statusCode, nx::utils::Url url)
            {
                if (statusCode != nx::network::http::StatusCode::ok)
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
    NX_LOGX(lm("Creating transaction connection to cloud_db at %1")
        .arg(url), cl_logDEBUG1);

    m_cloudUrl = url;
    m_cloudUrl.setPath(QString::fromUtf8(nx::cdb::api::kEc2EventsPath));
    m_cloudUrl.setUserName(m_commonModule->globalSettings()->cloudSystemId());
    m_cloudUrl.setPassword(m_commonModule->globalSettings()->cloudAuthKey());
    m_ec2CloudConnector->startDataSynchronization(m_cloudUrl);
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
