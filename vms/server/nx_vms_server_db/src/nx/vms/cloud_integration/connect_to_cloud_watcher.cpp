#include "connect_to_cloud_watcher.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/url/url_builder.h>
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

namespace {

struct CloudPeerInfo
{
    std::string id;
};

#define CloudPeerInfo_Fields (id)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (CloudPeerInfo),
    (json),
    _Fields)

static constexpr char kCloudPeerInfoPath[] = "/cdb/ec2/info";

} // namespace

//-------------------------------------------------------------------------------------------------

QnConnectToCloudWatcher::QnConnectToCloudWatcher(
    QnCommonModule* commonModule,
    AbstractEc2CloudConnector* ec2CloudConnector)
    :
    m_commonModule(commonModule),
    m_ec2CloudConnector(ec2CloudConnector),
    m_cdbEndPointFetcher(std::make_unique<CloudDbUrlFetcher>())
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
    m_cdbEndPointFetcher->executeInAioThreadSync(
        [this]()
        {
            m_cdbEndPointFetcher.reset();
            m_fetchPeerIdRequest.reset();
        });
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
                    // Try once more later.
                    metaObject()->invokeMethod(this, &QnConnectToCloudWatcher::restartTimer, Qt::QueuedConnection);
                    return;
                }

                setCloudDbUrl(url);
                metaObject()->invokeMethod(this, &QnConnectToCloudWatcher::at_updateConnection, Qt::QueuedConnection);
            });
    }
}

void QnConnectToCloudWatcher::addCloudPeer(const nx::utils::Url& url)
{
    m_cdbEndPointFetcher->post([this, url]() { fetchCloudPeerIdAsync(url); });
}

void QnConnectToCloudWatcher::fetchCloudPeerIdAsync(const nx::utils::Url& url)
{
    static constexpr auto kReadTimeout = std::chrono::seconds(17);

    NX_DEBUG(this, lm("Fetching cloud_db peer id using URL %1").arg(url));

    m_cloudPeerCandidateUrl = url;

    m_fetchPeerIdRequest = std::make_unique<nx::network::http::AsyncClient>();
    m_fetchPeerIdRequest->setResponseReadTimeout(kReadTimeout);
    m_fetchPeerIdRequest->setMessageBodyReadTimeout(kReadTimeout);
    m_fetchPeerIdRequest->bindToAioThread(m_cdbEndPointFetcher->getAioThread());
    auto fetchPeerIdUrl = nx::network::url::Builder(url)
        .appendPath(kCloudPeerInfoPath);
    m_fetchPeerIdRequest->doGet(
        fetchPeerIdUrl,
        [this]() { processFetchPeerIdResponse(); });
}

void QnConnectToCloudWatcher::processFetchPeerIdResponse()
{
    using namespace nx::network::http;

    auto fetchPeerIdRequest = std::exchange(m_fetchPeerIdRequest, nullptr);

    if (fetchPeerIdRequest->failed() ||
        !StatusCode::isSuccessCode(fetchPeerIdRequest->response()->statusLine.statusCode))
    {
        NX_INFO(this, lm("Failed to fetch peer from URL %1").args(m_cloudPeerCandidateUrl));
        // Try once more later.
        metaObject()->invokeMethod(this, &QnConnectToCloudWatcher::restartTimer, Qt::QueuedConnection);
        return;
    }

    auto msgBody = fetchPeerIdRequest->fetchMessageBodyBuffer();
    auto cloudPeerInfo = QJson::deserialized<CloudPeerInfo>(msgBody);
    if (cloudPeerInfo.id.empty())
    {
        NX_INFO(this, lm("Got empty cloud peer id from URL %1").args(m_cloudPeerCandidateUrl));
        // Try once more later.
        metaObject()->invokeMethod(this, &QnConnectToCloudWatcher::restartTimer, Qt::QueuedConnection);
        return;
    }

    metaObject()->invokeMethod(
        this,
        [this, id = cloudPeerInfo.id]() { connectToCloudPeer(id, m_cloudPeerCandidateUrl); },
        Qt::QueuedConnection);
}

void QnConnectToCloudWatcher::connectToCloudPeer(
    const std::string& peerId,
    const nx::utils::Url& url)
{
    NX_DEBUG(this, lm("Creating transaction connection to cloud_db at %1")
        .arg(url));

    m_cloudUrl = url;
    m_cloudUrl.setPath(QString::fromUtf8(nx::cloud::db::api::kEc2EventsPath));
    m_cloudUrl.setUserName(m_commonModule->globalSettings()->cloudSystemId());
    m_cloudUrl.setPassword(m_commonModule->globalSettings()->cloudAuthKey());
    m_ec2CloudConnector->startDataSynchronization(peerId, m_cloudUrl);
}

} // namespace cloud_integration
} // namespace vms
} // namespace nx
