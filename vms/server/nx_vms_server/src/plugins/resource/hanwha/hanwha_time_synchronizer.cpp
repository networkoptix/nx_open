#include "hanwha_time_synchronizer.h"

#include <map>
#include <QtCore/QTimeZone>

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <utils/common/synctime.h>

#include "hanwha_resource.h"

static constexpr std::chrono::minutes kRetryInterval(5); //< Used after failure.
static constexpr std::chrono::hours kResyncInterval(1); //< Used after successful check.
static constexpr std::chrono::seconds kAcceptableTimeDiff(5); //< Diff between device and mediaserver.

static constexpr bool isTimeZoneSyncRequired = false;
static QString getPosixTimeZone(const QTimeZone& timeZone)
{
    const auto offsetSeconds = timeZone.standardTimeOffset(QDateTime::currentDateTime());
    const auto offsetHours = double(offsetSeconds) / (60 * 60);
    auto posixTimeZone = lit("STWT%1").arg(offsetHours);

    // Day light time transitions do not work for all timezones:
    /*
    const QDateTime startOfTheYear(QDate(QDate::currentDate().year(), 0, 0));
    for (const auto& transition: timeZone.transitions(startOfTheYear, startOfTheYear.addYears(1)))
    {
        const auto& point = transition.atUtc;
        posixTimeZone += lm(",M%1.%2.0/%3").args(
            point.date().month(), point.date().day(), point.time());
    }
    */

    return posixTimeZone;
}

namespace nx {
namespace vms::server {
namespace plugins {

HanwhaTimeSyncronizer::HanwhaTimeSyncronizer()
{
    m_httpClient.bindToAioThread(m_timer.getAioThread());
    m_syncTymeConnection = QObject::connect(
        qnSyncTime, &QnSyncTime::timeChanged,
        [this]() { retryVerificationIn(std::chrono::milliseconds::zero()); });
}

HanwhaTimeSyncronizer::~HanwhaTimeSyncronizer()
{
    QObject::disconnect(m_syncTymeConnection);

    utils::promise<void> promise;
    m_timer.pleaseStop(
        [this, &promise]()
        {
            m_httpClient.pleaseStopSync();
            promise.set_value();
        });

    promise.get_future().wait();
}

void HanwhaTimeSyncronizer::start(HanwhaSharedResourceContext* resourceConext)
{
    utils::promise<void> promise;
    m_timer.post(
        [this, resourceConext, &promise]() mutable
        {
            if (m_resourceConext) // Already started.
            {
                if (!m_isAsyncClientInProgress)
                    verifyDateTime();

                return promise.set_value();
            }

            m_resourceConext = resourceConext;
            NX_DEBUG(this, lm("Started by %1").args(resourceConext));

            m_startPromises.push_back(&promise);
            verifyDateTime();
        });

    promise.get_future().wait();
}

void HanwhaTimeSyncronizer::setTimeSynchronizationEnabled(bool enabled)
{
    m_timeSynchronizationEnabled = enabled;
}

void HanwhaTimeSyncronizer::setTimeShiftHandler(UtcTimeShiftHandler handler)
{
    m_timer.post(
        [this, handler = std::move(handler)]() mutable
        {
            m_utcTimeShiftHandler = std::move(handler);
            if (m_utcTimeShiftHandler)
                m_utcTimeShiftHandler(m_utcTimeShift);
        });
}

static QDateTime toUtcDateTime(const QString& value)
{
    auto dt = QDateTime::fromString(value, "yyyy-MM-dd hh:mm:ss");
    dt.setTimeSpec(Qt::UTC);
    return dt;
}

void HanwhaTimeSyncronizer::verifyDateTime()
{
    NX_VERBOSE(this, "Verify device date time");
    doRequest(lit("view"), {}, /*isList*/ false,
        [this](HanwhaResponse response)
        {
            const auto promiseGuard = nx::utils::makeScopeGuard([this](){ fireStartPromises(); });
            if (!response.isSuccessful())
                return retryVerificationIn(kRetryInterval);

            try
            {
                const auto utcDateTime = toUtcDateTime(response.getOrThrow("UTCTime"));
                NX_VERBOSE(this, lm("Device UTC time: %1").args(utcDateTime));
                const auto localDateTime = toUtcDateTime(response.getOrThrow("LocalTime"));
                NX_VERBOSE(this, lm("Device local time: %1").args(localDateTime));

                const auto serverDateTime = qnSyncTime->currentDateTime();
                updateTimeShift(
                    std::chrono::milliseconds(utcDateTime.msecsTo(serverDateTime)),
                    std::chrono::milliseconds(utcDateTime.msecsTo(localDateTime)));

                const auto timeDiff = std::chrono::abs(m_utcTimeShift);
                if (m_timeSynchronizationEnabled && timeDiff > kAcceptableTimeDiff)
                {
                    NX_DEBUG(this, lm("Camera has %1 time which is %2 seconds different")
                        .args(utcDateTime, utcDateTime.secsTo(serverDateTime)));

                    return setDateTime(serverDateTime);
                }

                // TODO: Uncomment in case we need to synchronize time zone as well.
                // if (response.getOrThrow("POSIXTimeZone") != getPosixTimeZone(dateTime.timeZone()))
                // {
                //     NX_DEBUG(this, lm("Unexpected POSIXTimeZone: %1").arg(*posixTimeZone));
                //     return setDateTime(now);
                // }

            }
            catch (const std::runtime_error& exception)
            {
                NX_WARNING(this, exception.what());
                return retryVerificationIn(kRetryInterval);
            }

            NX_VERBOSE(this, "Current time diff is %1, which is ok", m_utcTimeShift);
            retryVerificationIn(kResyncInterval);
        });
}

void HanwhaTimeSyncronizer::setDateTime(const QDateTime& dateTime)
{
    const auto utc = dateTime.toUTC();
    const auto local = utc.addMSecs(std::chrono::milliseconds(m_timeZoneShift).count());
    std::map<QString, QString> params = {
        {"SyncType", "Manual"},

        {"Year", QString::number(local.date().year())},
        {"Month", QString::number(local.date().month())},
        {"Day", QString::number(local.date().day())},

        {"Hour", QString::number(local.time().hour())},
        {"Minute", QString::number(local.time().minute())},
        {"Second", QString::number(local.time().second())},

        // TODO: Uncomment in case we need to synchronize time zone as well.
        // {"POSIXTimeZone", getPosixTimeZone(dateTime.timeZone())}
    };

    NX_VERBOSE(this, lm("Setting %1").container(params));
    doRequest(lit("set"), std::move(params), /*isList*/ false,
        [this, utc](HanwhaResponse response)
        {
            if (!response.isSuccessful())
                return retryVerificationIn(kRetryInterval);

            NX_DEBUG(this, lm("Time is succesfully syncronized to %1").arg(utc));
            retryVerificationIn(kResyncInterval);
        });
}

void HanwhaTimeSyncronizer::retryVerificationIn(std::chrono::milliseconds timeout)
{
    if (!m_resourceConext)
        return;

    const auto verify = [this](){ verifyDateTime(); };
    if (timeout.count() == 0)
        return m_timer.post(verify);

    m_timer.start(timeout, verify);
}

void HanwhaTimeSyncronizer::updateTimeShift(
    std::chrono::milliseconds utcTimeShift,
    std::chrono::milliseconds timeZoneShift)
{
    m_utcTimeShift = utcTimeShift;
    m_timeZoneShift = timeZoneShift;
    NX_DEBUG(this, "Camera current time shift is %1, time zone shift is %2",
        utcTimeShift, timeZoneShift);

    if (m_utcTimeShiftHandler)
        m_utcTimeShiftHandler(utcTimeShift);
}

void HanwhaTimeSyncronizer::fireStartPromises()
{
    decltype(m_startPromises) promises;
    std::swap(promises, m_startPromises);
    for (const auto& p: promises)
        p->set_value();
}

void HanwhaTimeSyncronizer::doRequest(
    const QString& action, std::map<QString, QString> params,
    bool isList, utils::MoveOnlyFunc<void(HanwhaResponse)> handler)
{
    // TODO: Use m_resourceConext->requestSemaphore().
    const auto authenticator = m_resourceConext->authenticator();
    const auto url = HanwhaRequestHelper::buildRequestUrl(
        m_resourceConext->url(), lit("system"), lit("date"), action, params);

    m_httpClient.pleaseStopSync();
    m_httpClient.setUserName(authenticator.user());
    m_httpClient.setUserPassword(authenticator.password());
    m_isAsyncClientInProgress = true;
    m_httpClient.doGet(url,
        [this, url, isList, handler = std::move(handler)]()
        {
            m_isAsyncClientInProgress = false;
            if (!m_httpClient.hasRequestSucceeded())
            {
                NX_DEBUG(this, lm("Failed request: %1").arg(url));
                return handler(HanwhaResponse(
                    nx::network::http::StatusCode::serviceUnavailable, url.toString()));
            }

            auto body = m_httpClient.fetchMessageBodyBuffer();
            auto code = static_cast<nx::network::http::StatusCode::Value>(
                m_httpClient.response()->statusLine.statusCode);

            handler(HanwhaResponse(std::move(body), code, url.toString(), QString(), isList));
        });
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
