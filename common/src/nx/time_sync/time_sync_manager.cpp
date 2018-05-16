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
#include <nx/fusion/serialization/json.h>
#include <rest/server/json_rest_result.h>
#include <api/model/time_reply.h>

namespace nx {
namespace time_sync {

static const std::chrono::seconds kProxySocetTimeout(10);
static const std::chrono::minutes kDefaultTimeSyncInterval(10);
//const QString kTimeSyncUrlPath = QString::fromLatin1("/api/timeSync");
const QString kTimeSyncUrlPath = QString::fromLatin1("/api/gettime");
static const QByteArray kTimeDeltaParamName = "sync_time_delta";

TimeSyncManager::TimeSyncManager(
    QnCommonModule* commonModule)
    :
    QnCommonModuleAware(commonModule),
    m_systemClock(std::make_shared<SystemClock>()),
    m_steadyClock(std::make_shared<SteadyClock>()),
    m_timeSyncInterval(kDefaultTimeSyncInterval),
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
        m_timer->start(std::chrono::milliseconds(m_timeSyncInterval).count());
    });
    connect(m_thread, &QThread::finished, [this]() { m_timer->stop(); });

    connect(
        commonModule->globalSettings(), &QnGlobalSettings::timeSynchronizationSettingsChanged,
        this, [this]() 
        { 
            updateTime(); 
        });
}

void TimeSyncManager::setClock(
    const std::shared_ptr<AbstractSystemClock>& systemClock,
    const std::shared_ptr<AbstractSteadyClock>& steadyClock)
{
    m_systemClock = systemClock;
    m_steadyClock = steadyClock;
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

bool TimeSyncManager::loadTimeFromServer(const QnRoute& route)
{
    auto socket = connectToRemoteHost(route);
    if (!socket)
    {
        NX_WARNING(this, 
            lm("Can't read time from server %1. Can't establish connection to the remote host."));
        return false;
    }

    auto httpClient = std::make_unique<nx::network::http::HttpClient>();
    httpClient->setSocket(std::move(socket));
    nx::utils::Url url(lit("http://%1:%2%3").arg(route.addr.address.toString()).arg(route.addr.port).arg(kTimeSyncUrlPath));
    //url.setPath(kTimeSyncUrlPath);

    nx::utils::ElapsedTimer timer;
    timer.restart();
    bool success = httpClient->doGet(url);
    auto response = httpClient->fetchEntireMessageBody();
    if (!success || !response)
    {
        NX_WARNING(this, lm("Can't read time from server %1. Error: %2")
            .args(route.id.toString(), httpClient->lastSysErrorCode()));
        return false;
    }

    auto jsonResult = QJson::deserialized<QnJsonRestResult>(*response);
    QnTimeReply timeData;
    if (!QJson::deserialize(jsonResult.reply, &timeData))
    {
        NX_WARNING(this, lm("Can't deserialize time reply from server %1")
            .arg(route.id.toString()));
        return false;
    }
    
    auto newTime = std::chrono::milliseconds(timeData.utcTime - timer.elapsedMs() / 2);
    setSyncTime(newTime);
    NX_DEBUG(this, lm("Got time %1 from the neighbor server %2")
        .args(QDateTime::fromMSecsSinceEpoch(newTime.count()).toString(Qt::ISODate),
        route.id.toString()));
    return true;
}

bool TimeSyncManager::setSyncTime(std::chrono::milliseconds value)
{
    const auto minDeltaToSync = 
        commonModule()->globalSettings()->maxDifferenceBetweenSynchronizedAndInternetTime();
    const auto syncTime = getSyncTime();
    const auto timeDelta = value < syncTime ? syncTime - value : value - syncTime;
    if (timeDelta < minDeltaToSync)
        return false;

    setSyncTimeInternal(value);
    emit timeChanged(value.count());
    return true;
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

void TimeSyncManager::setTimeSyncInterval(std::chrono::milliseconds value)
{
    m_timeSyncInterval = value;
}

std::chrono::milliseconds TimeSyncManager::timeSyncInterval() const
{
    return m_timeSyncInterval;
}

} // namespace time_sync
} // namespace nx
