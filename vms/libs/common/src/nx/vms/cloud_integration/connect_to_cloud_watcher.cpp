#include "connect_to_cloud_watcher.h"

#include <nx/utils/std/cpp14.h>

#include <nx/cloud/db/api/ec2_request_paths.h>

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
        this, [this]()
        {
            if (!m_cloudUrl.isEmpty())
                m_ec2CloudConnector->stopDataSynchronization(); //< Sop connection with old parameters if exists.
            at_updateConnection();
        });
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
    QnMutexLocker lock(&m_mutex);
    m_cloudDbUrl = cloudDbUrl;
}

std::optional<nx::utils::Url> QnConnectToCloudWatcher::cloudDbUrl() const
{
    QnMutexLocker lock(&m_mutex);
    return m_cloudDbUrl;
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
        localInfo.data.flags.testFlag(api::RuntimeFlag::masterCloudSync) &&
        !m_commonModule->globalSettings()->cloudSystemId().isEmpty() &&
        !m_commonModule->globalSettings()->cloudAuthKey().isEmpty();

    NX_DEBUG(this, "Update needCloudConnect. Value=%1, cloudSystemId=%2, cloudAuthKey empty=%3",
        needCloudConnect,
        m_commonModule->globalSettings()->cloudSystemId(),
        m_commonModule->globalSettings()->cloudAuthKey().isEmpty());
    if (!needCloudConnect)
    {
        if (!m_cloudUrl.isEmpty())
            m_ec2CloudConnector->stopDataSynchronization();
        return;
    }

    auto cloudDbUrl = this->cloudDbUrl();
    if (cloudDbUrl)
    {
        addCloudPeer(*cloudDbUrl);
    }
    else
    {
        m_cdbEndPointFetcher->get(
            nx::network::http::AuthInfo(),
            [this](int statusCode, nx::utils::Url url)
            {
                if (statusCode != nx::network::http::StatusCode::ok)
                {
                    NX_WARNING(this, lm("Error fetching cloud_db endpoint. HTTP result: %1")
                        .arg(statusCode));
                    // try once more later
                    metaObject()->invokeMethod(this, &QnConnectToCloudWatcher::restartTimer, Qt::QueuedConnection);
                    return;
                }
                setCloudDbUrl(url);
                metaObject()->invokeMethod(this, &QnConnectToCloudWatcher::at_updateConnection, Qt::QueuedConnection);
            });
    }
}

void QnConnectToCloudWatcher::addCloudPeer(nx::utils::Url url)
{
    NX_DEBUG(this, lm("Creating transaction connection to cloud_db at %1")
        .arg(url));

    m_cloudUrl = url;
    m_cloudUrl.setPath(QString::fromUtf8(nx::cloud::db::api::kEc2EventsPath));
    m_cloudUrl.setUserName(m_commonModule->globalSettings()->cloudSystemId());
    m_cloudUrl.setPassword(m_commonModule->globalSettings()->cloudAuthKey());
    m_ec2CloudConnector->startDataSynchronization(m_cloudUrl);
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
