#include "time_sync_manager.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <common/common_module.h>
#include <api/global_settings.h>
#include <nx/utils/elapsed_timer.h>
#include <nx_ec/data/api_reverse_connection_data.h>
#include <nx/network/time/time_protocol_client.h>
#include <utils/common/rfc868_servers.h>
#include <nx/network/socket_factory.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/time.h>
#include <network/router.h>

namespace nx {
namespace time_sync {

static const std::chrono::seconds kProxySocetTimeout(10);
static const std::chrono::minutes kTimeSyncInterval(10);
const QString kTimeSyncUrlPath = QString::fromLatin1("/api/timeSync");
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
    const std::shared_ptr<AbstractSystemClock>& systemClock,
    const std::shared_ptr<AbstractSteadyClock>& steadyClock)
    :
    QnCommonModuleAware(commonModule),
    m_systemClock(systemClock ? systemClock : std::make_shared<SystemClock>()),
    m_steadyClock(steadyClock ? steadyClock : std::make_shared<SteadyClock>()),
    m_thread(new QThread())
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
        updateTime();
        m_timer->start(std::chrono::milliseconds(kTimeSyncInterval).count());
    });
    connect(m_thread, &QThread::finished, [this]() { m_timer->stop(); });

    connect(
        commonModule->globalSettings(), &QnGlobalSettings::timeSynchronizationSettingsChanged,
        this, [this]() 
        { 
            updateTime(); 
        });
}

TimeSyncManager::~TimeSyncManager()
{
    stop();
}

void TimeSyncManager::start()
{
    m_thread->start();
}

void TimeSyncManager::stop()
{
    m_thread->exit();
    m_thread->wait();
}

std::unique_ptr<nx::network::AbstractStreamSocket> TimeSyncManager::connectToRemoteHost(const QnRoute& route)
{
    auto socket = nx::network::SocketFactory::createStreamSocket(false);
    if (socket->connect(route.addr, nx::network::deprecated::kDefaultConnectTimeout))
        return socket;
    return std::unique_ptr<nx::network::AbstractStreamSocket>();
}

void TimeSyncManager::loadTimeFromLocalClock()
{
    auto newValue = m_systemClock->millisSinceEpoch();
    setSyncTime(newValue);
    NX_DEBUG(this, lm("Set time %1 from the local clock").
        arg(QDateTime::fromMSecsSinceEpoch(newValue.count()).toString(Qt::ISODate)));
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
    setSyncTime(newTime);
    NX_DEBUG(this, lm("Received time %1 from the neighbor server %2")
        .args(QDateTime::fromMSecsSinceEpoch(newTime.count()).toString(Qt::ISODate),
        route.id.toString()));
}

void TimeSyncManager::setSyncTime(std::chrono::milliseconds value)
{
    const auto minDeltaToSync = 
        commonModule()->globalSettings()->maxDifferenceBetweenSynchronizedAndInternetTime();
    const auto timeDelta = value - getSyncTime();
    if (timeDelta < minDeltaToSync)
        return;

    setSyncTimeInternal(value);
    emit timeChanged(value.count());
}

void TimeSyncManager::setSyncTimeInternal(std::chrono::milliseconds value)
{
    QnMutexLocker lock(&m_mutex);
    m_synchronizedTime = value;
    m_synchronizedOnClock = m_steadyClock->now();
}


std::chrono::milliseconds TimeSyncManager::getSyncTime() const
{
    QnMutexLocker lock(&m_mutex);

    auto elapsed = m_steadyClock->now() - m_synchronizedOnClock;
    return m_synchronizedTime + elapsed;
}

void TimeSyncManager::doPeriodicTasks()
{
    updateTime();
}

} // namespace time_sync
} // namespace nx
