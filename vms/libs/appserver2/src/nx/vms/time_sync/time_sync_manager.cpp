#include "time_sync_manager.h"

#include <api/global_settings.h>
#include <api/model/time_reply.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/router.h>
#include <rest/server/json_rest_result.h>
#include <utils/common/rfc868_servers.h>

#include <nx/fusion/serialization/json.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_client.h>
#include <nx/network/socket_factory.h>
#include <nx/network/time/time_protocol_client.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/utils/time.h>
#include <common/static_common_module.h>

namespace nx {
namespace vms {
namespace time_sync {

const QString TimeSyncManager::kTimeSyncUrlPath(lit("/api/synchronizedTime"));
const std::chrono::milliseconds TimeSyncManager::kMaxJitterForLocalClock(250);

TimeSyncManager::TimeSyncManager(
    QnCommonModule* commonModule)
    :
    QnCommonModuleAware(commonModule),
    m_systemClock(std::make_shared<SystemClock>()),
    m_steadyClock(std::make_shared<SteadyClock>()),
    m_thread(new QThread())
{
    moveToThread(m_thread.get());

    connect(m_thread.get(), &QThread::started,
        [this]()
        {
            if (!m_timer)
            {
                m_timer.reset(new QTimer());
                connect(m_timer.get(), &QTimer::timeout, this, &TimeSyncManager::doPeriodicTasks);
            }
            updateTime();
            auto globalSettings = this->commonModule()->globalSettings();
            m_timer->start(
                std::chrono::milliseconds(globalSettings->osTimeChangeCheckPeriod()).count());
        });
    connect(m_thread.get(), &QThread::finished, [this]() { m_timer->stop(); });

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

std::unique_ptr<nx::network::AbstractStreamSocket> TimeSyncManager::connectToRemoteHost(
    const QnRoute& route,
    bool sslRequired)
{
    auto socket = nx::network::SocketFactory::createStreamSocket(sslRequired);
    if (socket->connect(route.addr, nx::network::deprecated::kDefaultConnectTimeout))
        return socket;
    return std::unique_ptr<nx::network::AbstractStreamSocket>();
}

void TimeSyncManager::loadTimeFromLocalClock()
{
    auto newValue = m_systemClock->millisSinceEpoch();

    if (setSyncTime(newValue, kMaxJitterForLocalClock))
    {
        NX_DEBUG(this, lm("Set time %1 from the local clock")
            .arg(QDateTime::fromMSecsSinceEpoch(newValue.count()).toString(Qt::ISODate)));
    }
    m_isTimeTakenFromInternet = false;
}

TimeSyncManager::Result TimeSyncManager::loadTimeFromServer(const QnRoute& route)
{
    auto server = commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(route.id);
    if (!server)
    {
        NX_DEBUG(this,
            lm("Server %1 is not known yet. Postpone time sync request").arg(qnStaticCommon->moduleDisplayName(route.id)));
        return Result::error;
    }

    nx::utils::Url url(server->getApiUrl());
    url.setHost(route.addr.address.toString());
    url.setPort(route.addr.port);
    url.setPath(kTimeSyncUrlPath);

    // Server v.4.0 pass this method as unauthorized.
    // But client may open connection to the server v.3.2.
    // It doesn't know about this method and requests authorization by default.
    const auto userName = commonModule()->currentUrl().userName();
    const auto password = commonModule()->currentUrl().password();
    if (!userName.isEmpty() && !password.isEmpty())
    {
        url.setUserName(userName);
        url.setPassword(password);
    }

    const bool sslRequired = url.scheme() == nx::network::http::kSecureUrlSchemeName;
    auto socket = connectToRemoteHost(route, sslRequired);

    if (!socket)
    {
        NX_WARNING(this,
            lm("Can't read time from server %1. Can't establish connection to the remote host.")
            .arg(qnStaticCommon->moduleDisplayName(route.id)));
        return Result::error;
    }
    auto maxRtt = commonModule()->globalSettings()->maxDifferenceBetweenSynchronizedAndLocalTime();

    auto httpClient = std::make_unique<nx::network::http::HttpClient>(std::move(socket));
    httpClient->setResponseReadTimeout(std::chrono::milliseconds(maxRtt));
    httpClient->addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, route.id.toByteArray());

    nx::utils::ElapsedTimer rttTimer;
    // With gateway repeat request twice to make sure we have opened tunnel to the target server.
    // That way it is possible to reduce rtt.
    int iterations = route.gatewayId.isNull() ? 1 : 2;
    std::optional<QByteArray> response;
    for (int i = 0; i < iterations; ++i)
    {
        rttTimer.restart();
        if (httpClient->doGet(url))
            response = httpClient->fetchEntireMessageBody();
        else
            response.reset();
        if (!response)
        {
            if (iterations == 1 && httpClient->lastSysErrorCode() == SystemError::noError)
                ++iterations;
            NX_WARNING(this,
                "Can't read time from server %1, iteration %2 of %3, timeout: %4, error: %5",
                qnStaticCommon->moduleDisplayName(route.id),
                i + 1, iterations,
                maxRtt,
                httpClient->lastSysErrorCode());
        }
    }
    if (!response)
        return Result::error;

    if (httpClient->contentLocationUrl() != httpClient->url())
    {
        // In case of client v4.0 connect to the server v3.2 it don't understand this request
        // and redirect it to the index.html root page instead.
        NX_DEBUG(this, "Can not synchronized time with incompatible server");
        return Result::incompatibleServer;
    }

    auto jsonResult = QJson::deserialized<QnJsonRestResult>(*response);
    SyncTimeData timeData;
    if (!QJson::deserialize(jsonResult.reply, &timeData))
    {
        NX_WARNING(this, lm("Can't deserialize time reply from server %1")
            .arg(qnStaticCommon->moduleDisplayName(route.id)));
        return Result::error;
    }

    const std::chrono::milliseconds rtt = rttTimer.elapsed();
    auto newTime = std::chrono::milliseconds(timeData.utcTimeMs - rtt.count() / 2);
    bool syncWithInternel = commonModule()->globalSettings()->primaryTimeServer().isNull();
    if (syncWithInternel && !timeData.isTakenFromInternet)
        return Result::error; //< Target server is not ready yet. Time is not taken from internet yet. Repeat later.
    m_isTimeTakenFromInternet = timeData.isTakenFromInternet;
    if (rtt > maxRtt)
        return Result::error; //< Too big rtt. Try again.
    const auto ownGuid = commonModule()->moduleGUID();
    NX_DEBUG(this, lm("Got time %1 (%2 <-- %3), rtt=%4")
        .args(
            QDateTime::fromMSecsSinceEpoch(newTime.count()).toString(Qt::ISODate),
            qnStaticCommon->moduleDisplayName(ownGuid),
            qnStaticCommon->moduleDisplayName(route.id),
            rtt));
    if (!setSyncTime(newTime, rtt))
    {
        NX_DEBUG(this, lm("Server %1 ignore new time %2 because of small delta.")
            .arg(qnStaticCommon->moduleDisplayName(commonModule()->moduleGUID()))
            .arg(QDateTime::fromMSecsSinceEpoch(newTime.count()).toString(Qt::ISODate)));
    }
    return Result::ok;
}

bool TimeSyncManager::setSyncTime(std::chrono::milliseconds value, std::chrono::milliseconds rtt)
{
    const auto syncTime = getSyncTime();
    const auto timeDelta = value < syncTime ? syncTime - value : value - syncTime;
    if (timeDelta < rtt)
        return false;

    NX_INFO(this,
        lm("Set sync time to the new value %1. Difference between new and old value is %2")
            .arg(value.count())
            .arg(value - syncTime));

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

std::chrono::milliseconds TimeSyncManager::getSyncTime(bool* outIsTimeTakenFromInternet) const
{
    QnMutexLocker lock(&m_mutex);
    if (outIsTimeTakenFromInternet)
        *outIsTimeTakenFromInternet = m_isTimeTakenFromInternet;
    if (m_synchronizedTime == std::chrono::milliseconds::zero())
        return m_systemClock->millisSinceEpoch(); //< Network sync is not initialized yet.
    auto elapsed = m_steadyClock->now() - m_synchronizedOnClock;
    return m_synchronizedTime + elapsed;
}

void TimeSyncManager::doPeriodicTasks()
{
    updateTime();
}

QString TimeSyncManager::idForToStringFromPtr() const
{
    return qnStaticCommon->moduleDisplayName(this->commonModule()->moduleGUID());
}

} // namespace time_sync
} // namespace vms
} // namespace nx
