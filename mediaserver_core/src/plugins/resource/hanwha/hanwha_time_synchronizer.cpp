#include "hanwha_time_synchronizer.h"

#include <QtCore/QTimeZone>

#include <utils/common/synctime.h>
#include <nx/utils/log/log.h>

#include "hanwha_resource.h"

static constexpr std::chrono::minutes kRetryInterval(5);
static constexpr std::chrono::hours kResyncInterval(1);
static constexpr qint64 kAcceptableTimeDiffSecs = 5;

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
namespace mediaserver_core {
namespace plugins {

HanwhaTimeSyncronizer::HanwhaTimeSyncronizer(const HanwhaResource* resource):
    m_resource(resource)
{
    m_httpClient.bindToAioThread(m_timer.getAioThread());
    verifyDateTime();

    m_syncTymeConnection = QObject::connect(
        qnSyncTime, &QnSyncTime::timeChanged,
        [this]() { retryVerificationIn(std::chrono::milliseconds::zero()); });

    NX_DEBUG(this, lm("Initializer for %1").arg(m_resource->getUrl()));
}

HanwhaTimeSyncronizer::~HanwhaTimeSyncronizer()
{
    QObject::disconnect(m_syncTymeConnection);

    utils::promise<void> promise;
    m_timer.pleaseStop(
        [this, &promise]()
        {
            m_httpClient.pleaseStopSync();
        });

    promise.get_future().wait();
}

static QDateTime toUtcDateTime(const QString& value)
{
    auto dt = QDateTime::fromString(value, "yyyy-MM-dd hh:mm:ss");
    dt.setTimeSpec(Qt::UTC);
    return dt;
}

void HanwhaTimeSyncronizer::verifyDateTime()
{
    doRequest(lit("view"), {}, /*isList*/ false,
        [this](HanwhaResponse response)
        {
            const auto utcTime = response.parameter<QString>("UTCTime");
            if (!utcTime)
            {
                NX_WARNING(this, lm("No UTCTime in response from %1").arg(response.requestUrl()));
                return retryVerificationIn(kRetryInterval);
            }

            const auto localTime = response.parameter<QString>("LocalTime");
            if (!localTime)
            {
                NX_WARNING(this, lm("No LocalTime in response from %1").arg(response.requestUrl()));
                return retryVerificationIn(kRetryInterval);
            }

            const auto utcDateTime = toUtcDateTime(*utcTime);
            const auto localDateTime = toUtcDateTime(*localTime);

            // We do not know time zone, so we use UTC to compare with real UTC time.
            m_currentTimeZoneShiftSecs = utcDateTime.secsTo(localDateTime);
            NX_VERBOSE(this, lm("Camera current time shift %1 secs from UTC")
                .arg(m_currentTimeZoneShiftSecs));

            const auto serverDateTime = qnSyncTime->currentDateTime();
            const auto timeDiffSecs = std::abs((int) utcDateTime.secsTo(serverDateTime));
            if (timeDiffSecs > kAcceptableTimeDiffSecs)
            {
                NX_DEBUG(this, lm("Camera has %1 time which is %2 seconds different")
                    .args(utcDateTime, utcDateTime.secsTo(serverDateTime)));

                return setDateTime(serverDateTime);
            }

            // TODO: Uncomment in case we need to maintain timezone as well.
            /*
            const auto posixTimeZone = response.parameter<QString>("POSIXTimeZone");
            if (!posixTimeZone)
            {
                NX_WARNING(this, lm("No POSIXTimeZone in response from %1").arg(response.requestUrl()));
                return retryVerificationIn(kRetryInterval);
            }

            if (*posixTimeZone != getPosixTimeZone(dateTime.timeZone()))
            {
                NX_DEBUG(this, lm("Unexpected POSIXTimeZone: %1").arg(*posixTimeZone));
                return setDateTime(now);
            }
            */

            NX_VERBOSE(this, lm("Current time diff is %1 secs which is ok").arg(timeDiffSecs));
            retryVerificationIn(kResyncInterval);
        });
}

void HanwhaTimeSyncronizer::setDateTime(const QDateTime& dateTime)
{
    const auto utc = dateTime.toUTC();
    const auto local = utc.addSecs(m_currentTimeZoneShiftSecs);

    std::map<QString, QString> params;
    params["SyncType"] = lit("Manual");

    // TODO: If we want to change time zone, time should also be adjasted to new time zone.
    // params["POSIXTimeZone"] = getPosixTimeZone(dateTime.timeZone());

    params["Year"] = QString::number(local.date().year());
    params["Month"] = QString::number(local.date().month());
    params["Day"] = QString::number(local.date().day());

    params["Hour"] = QString::number(local.time().hour());
    params["Minute"] = QString::number(local.time().minute());
    params["Second"] = QString::number(local.time().second());

    NX_VERBOSE(this, lm("Setting %1").container(params));
    doRequest(lit("set"), std::move(params), /*isList*/ false,
        [this, utc](HanwhaResponse /*response*/)
        {
            NX_DEBUG(this, lm("Time is succesfully syncronized to %1").arg(utc));
            retryVerificationIn(kResyncInterval);
        });
}

void HanwhaTimeSyncronizer::retryVerificationIn(std::chrono::milliseconds timeout)
{
    const auto verify = [this](){ verifyDateTime(); };
    if (timeout.count() == 0)
        return m_timer.post(verify);

    m_timer.start(timeout, verify);
}

void HanwhaTimeSyncronizer::doRequest(
    const QString& action, std::map<QString, QString> params,
    bool isList, utils::MoveOnlyFunc<void(HanwhaResponse)> handler)
{
    const auto url = HanwhaRequestHelper::buildRequestUrl(
        m_resource->getUrl(), lit("system"), lit("date"), action, params);

    const auto auth = m_resource->getAuth();
    m_httpClient.setUserName(auth.user());
    m_httpClient.setUserPassword(auth.password());
    m_httpClient.doGet(url,
        [this, url, isList, handler=std::move(handler)]()
        {
            if (!m_httpClient.hasRequestSuccesed())
            {
                NX_DEBUG(this, lm("Failed request: %1").arg(url));
                return retryVerificationIn(kRetryInterval);
            }

            auto body = m_httpClient.fetchMessageBodyBuffer();
            auto code = static_cast<nx_http::StatusCode::Value>(
                m_httpClient.response()->statusLine.statusCode);

            handler(HanwhaResponse(std::move(body), code, url.toString(), QString(), isList));
        });
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
