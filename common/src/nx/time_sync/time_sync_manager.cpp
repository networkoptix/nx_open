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
#include <nx/network/http/custom_headers.h>

namespace nx {
namespace time_sync {

const QString TimeSyncManager::kTimeSyncUrlPath(lit("/api/synchronizedTime"));

TimeSyncManager::TimeSyncManager(
    QnCommonModule* commonModule)
    :
    QnCommonModuleAware(commonModule),
    m_systemClock(std::make_shared<SystemClock>()),
    m_steadyClock(std::make_shared<SteadyClock>()),
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
            auto globalSettings = this->commonModule()->globalSettings();
            m_timer->start(
                std::chrono::milliseconds(globalSettings->osTimeChangeCheckPeriod()).count());
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
    static const std::chrono::milliseconds kMaxJitterForLocalClock(250);

    setSyncTime(newValue, kMaxJitterForLocalClock);
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
    auto maxRtt = commonModule()->globalSettings()->maxDifferenceBetweenSynchronizedAndLocalTime();

    auto httpClient = std::make_unique<nx::network::http::HttpClient>();
    httpClient->setSocket(std::move(socket));
    httpClient->setResponseReadTimeoutMs(std::chrono::milliseconds(maxRtt).count());
    httpClient->addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, route.id.toByteArray());
    auto server = commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(route.id);
    if (!server)
    {
        NX_WARNING(this,
            lm("Can't find server with id %1").arg(route.id));
        return false;
    }
    nx::utils::Url url(server->getApiUrl());
    url.setHost(route.addr.address.toString());
    url.setPort(route.addr.port);
    url.setPath(kTimeSyncUrlPath);

    nx::utils::ElapsedTimer rttTimer;
    // With gateway repeat request twice to make sure we have opened tunnel to the target server.
    // That way it is possible to reduce rtt.
    const int iterations = route.gatewayId.isNull() ? 1 : 2;
    boost::optional<QByteArray> response;
    for (int i = 0; i < iterations; ++i)
    {
        rttTimer.restart();
        bool success = httpClient->doGet(url);
        response = httpClient->fetchEntireMessageBody();
        if (!success || !response)
        {
            NX_WARNING(this, lm("Can't read time from server %1. Error: %2")
                .args(route.id.toString(), httpClient->lastSysErrorCode()));
            return false;
        }
    }

    auto jsonResult = QJson::deserialized<QnJsonRestResult>(*response);
    SyncTimeData timeData;
    if (!QJson::deserialize(jsonResult.reply, &timeData))
    {
        NX_WARNING(this, lm("Can't deserialize time reply from server %1")
            .arg(route.id.toString()));
        return false;
    }

    const std::chrono::milliseconds rtt = rttTimer.elapsed();
    auto newTime = std::chrono::milliseconds(timeData.utcTimeMs - rtt.count() / 2);
    bool syncWithInternel = commonModule()->globalSettings()->isSynchronizingTimeWithInternet();
    if (syncWithInternel && !timeData.isTakenFromInternet)
        return false; //< Target server is not ready yet. Time is not taken from internet yet. Repeat later.
    if (rtt > maxRtt)
        return false; //< Too big rtt. Try again.
    setSyncTime(newTime, rtt);
    NX_DEBUG(this, lm("Got time %1 from the neighbor server %2")
        .args(QDateTime::fromMSecsSinceEpoch(newTime.count()).toString(Qt::ISODate),
        route.id.toString()));
    return true;
}

bool TimeSyncManager::setSyncTime(std::chrono::milliseconds value, std::chrono::milliseconds rtt)
{
    const auto syncTime = getSyncTime();
    const auto timeDelta = value < syncTime ? syncTime - value : value - syncTime;
    if (timeDelta < rtt / 2)
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
    if (m_synchronizedTime == std::chrono::milliseconds::zero())
        return m_systemClock->millisSinceEpoch(); //< Network sync is not initialized yet.

    auto elapsed = m_steadyClock->now() - m_synchronizedOnClock;
    return m_synchronizedTime + elapsed;
}

void TimeSyncManager::doPeriodicTasks()
{
    auto globalSettings = this->commonModule()->globalSettings();
    if (globalSettings->isTimeSynchronizationEnabled())
        updateTime();
}

} // namespace time_sync
} // namespace nx
