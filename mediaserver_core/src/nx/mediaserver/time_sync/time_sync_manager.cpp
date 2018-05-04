#include "time_sync_manager.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <api/global_settings.h>
#include <nx/utils/elapsed_timer.h>
#include <nx_ec/data/api_reverse_connection_data.h>
#include <media_server/media_server_module.h>
#include <nx/mediaserver/reverse_connection_manager.h>
#include <nx/network/time/time_protocol_client.h>
#include <utils/common/rfc868_servers.h>
#include <nx/network/socket_factory.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/time.h>

namespace nx {
namespace mediaserver {

static const std::chrono::seconds kProxySocetTimeout(10);
const QString kTimeSyncUrlPath = QString::fromLatin1("ec2/timeSync");
static const QByteArray kTimeDeltaParamName = "sync_time_delta";

class SystemClock: public AbstractSystemClock
{
public:
    virtual std::chrono::milliseconds millisSinceEpoch() override
    {
        return nx::utils::millisSinceEpoch();
    }
};

class SteadyClock: public AbstractSteadyClock
{
public:
    virtual std::chrono::milliseconds now() override
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch());
    }
};

TimeSyncManager::TimeSyncManager(
    QnCommonModule* commonModule,
    ReverseConnectionManager* reverseConnectionManager,
    const std::shared_ptr<AbstractSystemClock>& systemClock,
    const std::shared_ptr<AbstractSteadyClock>& steadyClock)
    :
    QnCommonModuleAware(commonModule),
    m_systemClock(systemClock ? systemClock : std::make_shared<SystemClock>()),
    m_steadyClock(steadyClock ? steadyClock : std::make_shared<SteadyClock>()),
    m_thread(new QThread()),
    m_reverseConnectionManager(reverseConnectionManager)
{
    moveToThread(m_thread);
        
    connect(m_thread, &QThread::started,
        [this]()
    {
        if (!m_timer)
        {
            m_timer = new QTimer();
            connect(m_timer, &QTimer::timeout, this, &TimeSyncManager::doPeriodicTasks);
        }
        m_timer->start(1000);
    });
    connect(m_thread, &QThread::finished, [this]() { m_timer->stop(); });

    initializeTimeFetcher();
}

TimeSyncManager::~TimeSyncManager()
{
    stop();
}

void TimeSyncManager::start()
{
    ec2::ApiMiscData deltaData;
    auto connection = commonModule()->ec2Connection();
    auto miscManager = connection->getMiscManager(Qn::kSystemAccess);
    auto dbResult = miscManager->getMiscParamSync(kTimeDeltaParamName, &deltaData);
    if (dbResult  != ec2::ErrorCode::ok)
    {
        NX_WARNING(this, "Failed to load time delta parameter from the database");
    }
    auto timeDelta = std::chrono::milliseconds(deltaData.value.toLongLong());
    setTime(m_systemClock->millisSinceEpoch() + timeDelta);

    connection->getMiscManager(Qn::kSystemAccess)->saveMiscParam(
        deltaData, this,
        [this](int /*reqID*/, ec2::ErrorCode errCode)
    {
        if (errCode != ec2::ErrorCode::ok)
            NX_WARNING(this, lm("Failed to save time delta data to the database"));
    });

    m_thread->start();
}

void TimeSyncManager::stop()
{
    m_thread->exit();
    m_thread->wait();

    if (m_internetTimeSynchronizer)
    {
        m_internetTimeSynchronizer->pleaseStopSync();
        m_internetTimeSynchronizer.reset();
    }
}

void TimeSyncManager::initializeTimeFetcher()
{
    auto meanTimerFetcher = std::make_unique<nx::network::MeanTimeFetcher>();
    for (const char* timeServer: RFC868_SERVERS)
    {
        meanTimerFetcher->addTimeFetcher(
            std::make_unique<nx::network::TimeProtocolClient>(
                QLatin1String(timeServer)));
    }

    m_internetTimeSynchronizer = std::move(meanTimerFetcher);
}

QnMediaServerResourcePtr TimeSyncManager::getPrimaryTimeServer()
{
    auto resourcePool = commonModule()->resourcePool();
    auto settings = commonModule()->globalSettings();
    QnUuid primaryTimeServer = settings->primaryTimeServer();
    if (!primaryTimeServer.isNull())
        return resourcePool->getResourceById<QnMediaServerResource>(primaryTimeServer); //< User-defined time server.
    
    // Automatically select primary time server.
    auto servers = resourcePool->getAllServers(Qn::Online);
    if (servers.isEmpty())
        return QnMediaServerResourcePtr();
    std::sort(servers.begin(), servers.end(),
        [](const QnMediaServerResourcePtr& left, const QnMediaServerResourcePtr& right)
        {
            bool leftHasInternet = left->getServerFlags().testFlag(Qn::SF_HasPublicIP);
            bool rightHasInternet = right->getServerFlags().testFlag(Qn::SF_HasPublicIP);
            if (leftHasInternet != rightHasInternet)
                return leftHasInternet < rightHasInternet;
            return left->getId() < right->getId();
        });
    return servers.front();
}

std::unique_ptr<nx::network::AbstractStreamSocket> TimeSyncManager::connectToRemoteHost(const QnRoute& route)
{
    if (route.reverseConnect && m_reverseConnectionManager)
    {
        const auto& target = route.gatewayId.isNull() ? route.id : route.gatewayId;
        return m_reverseConnectionManager->getProxySocket(target, kProxySocetTimeout);
    }
    else
    {
        auto socket = nx::network::SocketFactory::createStreamSocket(false);
        if (socket->connect(
            route.addr,
            nx::network::deprecated::kDefaultConnectTimeout))
        {
            return std::unique_ptr<nx::network::AbstractStreamSocket>();
        }
    }
    return std::unique_ptr<nx::network::AbstractStreamSocket>();
}

void TimeSyncManager::loadTimeFromServer(const QnRoute& route)
{
    auto socket = connectToRemoteHost(route);
    if (!socket)
        return;

    auto httpClient = std::make_unique<nx::network::http::HttpClient>();
    httpClient->setSocket(std::move(socket));
    nx::utils::Url url;
    url.setPath(kTimeSyncUrlPath);

    nx::utils::ElapsedTimer timer;
    timer.restart();
    if (!httpClient->doGet(url))
        return;
    auto response = httpClient->fetchEntireMessageBody();
    if (!response || response->isEmpty())
        return;

    auto newTime = std::chrono::milliseconds(response->toLongLong() - timer.elapsedMs() / 2);
    setTime(newTime);
    NX_DEBUG(this, lm("Received time %1 from the neighbor server %2")
        .args(QDateTime::fromMSecsSinceEpoch(newTime.count()).toString(Qt::ISODate),
        route.id.toString()));
}

void TimeSyncManager::loadTimeFromInternet()
{
    if (m_internetSyncInProgress.exchange(true))
        return; //< Sync already in progress.

    m_internetTimeSynchronizer->getTimeAsync(
        [this](const qint64 newValue, SystemError::ErrorCode errorCode)
        {
            if (errorCode)
            {
                NX_WARNING(this, lm("Failed to get time from the internet. %1")
                    .arg(SystemError::toString(errorCode)));
                return;
            }
            setTime(std::chrono::milliseconds(newValue));
            NX_DEBUG(this, lm("Received time %1 from the internet").
                arg(QDateTime::fromMSecsSinceEpoch(newValue).toString(Qt::ISODate)));
            m_internetSyncInProgress = false;
        });
}

void TimeSyncManager::loadTimeFromLocalClock()
{
    auto newValue = m_systemClock->millisSinceEpoch();
    setTime(newValue);
    NX_DEBUG(this, lm("Set time %1 from the local clock").
        arg(QDateTime::fromMSecsSinceEpoch(newValue.count()).toString(Qt::ISODate)));
}

void TimeSyncManager::setTime(std::chrono::milliseconds value)
{
    const auto minDeltaToSync = 
        commonModule()->globalSettings()->maxDifferenceBetweenSynchronizedAndInternetTime();
    const auto timeDelta = value - getTime();
    if (timeDelta < minDeltaToSync)
        return;

    const auto systemTimeDeltaMs = value.count() - m_systemClock->millisSinceEpoch().count();
    auto connection = commonModule()->ec2Connection();
    
    ec2::ApiMiscData deltaData(
        kTimeDeltaParamName,
        QByteArray::number(systemTimeDeltaMs));

    connection->getMiscManager(Qn::kSystemAccess)->saveMiscParam(
        deltaData, this, 
        [this](int /*reqID*/, ec2::ErrorCode errCode)
        {
        if (errCode != ec2::ErrorCode::ok)
            NX_WARNING(this, lm("Failed to save time delta data to the database"));
        });


    QnMutexLocker lock(&m_mutex);
    m_synchronizedTime = value;
    m_synchronizedOnClock = m_steadyClock->now();
}

std::chrono::milliseconds TimeSyncManager::getTime() const
{
    QnMutexLocker lock(&m_mutex);

    auto elapsed = m_steadyClock->now() - m_synchronizedOnClock;
    return m_synchronizedTime + elapsed;
}

void TimeSyncManager::doPeriodicTasks()
{
    auto server = getPrimaryTimeServer();
    if (!server)
        return;

    bool syncWithInternel = commonModule()->globalSettings()->isSynchronizingTimeWithInternet();
    if (server->getId() == commonModule()->moduleGUID())
    {
        if (syncWithInternel)
            loadTimeFromInternet();
        else
            loadTimeFromLocalClock();
    }
    else
    {
        auto route = commonModule()->router()->routeTo(server->getId());
        if (route.isValid())
            loadTimeFromServer(route);
    }
}

} // namespace mediaserver
} // namespace nx
