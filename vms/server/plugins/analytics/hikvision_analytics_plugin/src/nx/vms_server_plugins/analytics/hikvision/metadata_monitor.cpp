#include "bytestream_filter.h"
#include "metadata_monitor.h"

#include <QtCore/QUrlQuery>

#include <chrono>
#include <algorithm>
#include <set>

#include <nx/fusion/serialization_format.h>
#include <nx/network/http/buffer_source.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/std/cpp14.h>

#include "attributes_parser.h"
#include "string_helper.h"
#include "metadata_parser.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

namespace {

const QString kMonitorUrlTemplate("/ISAPI/Event/notification/alertStream");
const QString kLprUrlTemplate("/ISAPI/Traffic/channels/1/vehicleDetect/plates");

constexpr int kDefaultHttpPort = 80;
constexpr std::chrono::seconds kLprHttpRequestTimeout(15);
constexpr std::chrono::minutes kKeepAliveTimeout(2);
constexpr std::chrono::seconds kMinReopenInterval(10);
constexpr std::chrono::seconds kLprRequestsTimeout(2);
constexpr std::chrono::seconds kExpiredEventTimeout(5);
constexpr int kAlarmLevels = 2;
constexpr int kMaxSupportedRegionCount = 4;

std::vector<QString> addEventTypesForObjectTracking(
    std::vector<QString> ids, const std::vector<QString>& objectTypes)
{
    std::set<QString> uniqueIds(ids.begin(), ids.end());

    if (std::find(objectTypes.begin(), objectTypes.end(), "nx.hikvision.ThermalObject")
            != objectTypes.end())
    {
        constexpr char kPattern[] = "nx.hikvision.Alarm%1Thermal%2";
        for (int i = 1; i <= kAlarmLevels; ++i)
        {
            uniqueIds.emplace(nx::format(kPattern, i, "Any"));
            for (int j = 1; j <= kMaxSupportedRegionCount; ++j)
                uniqueIds.emplace(nx::format(kPattern, i, j));
        }
    }

    ids.assign(uniqueIds.begin(), uniqueIds.end());

    return ids;
}

} // namespace

HikvisionMetadataMonitor::HikvisionMetadataMonitor(
    const Hikvision::EngineManifest& manifest,
    const nx::vms::api::analytics::DeviceAgentManifest& deviceManifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    const std::vector<QString>& eventTypes,
    const std::vector<QString>& objectTypes)
    :
    m_manifest(manifest),
    m_deviceManifest(deviceManifest),
    m_monitorUrl(buildMonitoringUrl(url, eventTypes, objectTypes)),
    m_lprUrl(buildLprUrl(url)),
    m_auth(auth)
{
    m_monitorTimer.bindToAioThread(m_timer.getAioThread());
    m_lprTimer.bindToAioThread(m_timer.getAioThread());
}

HikvisionMetadataMonitor::~HikvisionMetadataMonitor() { stopMonitoring(); }

void HikvisionMetadataMonitor::setDeviceInfo(const QString& deviceName, const QString& id)
{
    m_deviceName = deviceName;
    m_deviceId = id;
}

void HikvisionMetadataMonitor::startMonitoring()
{
    NX_VERBOSE(this, "Started");
    m_timer.post([this]() { initMonitorUnsafe(); });
}

void HikvisionMetadataMonitor::stopMonitoring()
{
    nx::utils::promise<void> promise;
    m_timer.post([this, &promise]() {
        if (m_monitorHttpClient)
            m_monitorHttpClient->pleaseStopSync();
        if (m_lprHttpClient)
            m_lprHttpClient->pleaseStopSync();

        m_monitorTimer.pleaseStopSync();
        m_lprTimer.pleaseStopSync();
        promise.set_value();
    });

    promise.get_future().wait();
    NX_VERBOSE(this, "Stopped");
}

void HikvisionMetadataMonitor::addHandler(const QString& handlerId, const Handler& handler)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers[handlerId] = handler;
}

void HikvisionMetadataMonitor::removeHandler(const QString& handlerId)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.remove(handlerId);
}

void HikvisionMetadataMonitor::clearHandlers()
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.clear();
}

nx::utils::Url HikvisionMetadataMonitor::buildMonitoringUrl(
    const nx::utils::Url& resourceUrl,
    const std::vector<QString>& eventTypes,
    const std::vector<QString>& objectTypes) const
{
    const int channel =
        std::max(1, QUrlQuery(resourceUrl.query()).queryItemValue("channel").toInt());
    QString eventListIds;
    for (const auto& eventTypeId: addEventTypesForObjectTracking(eventTypes, objectTypes))
    {
        const auto name = m_manifest.eventTypeById(eventTypeId).internalName;
        eventListIds += lit("/%1-%2").arg(name).arg(channel).toLower();
    }

    nx::utils::Url monitorUrl(resourceUrl);
    monitorUrl.setPath(kMonitorUrlTemplate);
    return monitorUrl;
}

nx::utils::Url HikvisionMetadataMonitor::buildLprUrl(const nx::utils::Url& resourceUrl) const
{
    nx::utils::Url lprUrl(resourceUrl);
    lprUrl.setPath(kLprUrlTemplate);
    return lprUrl;
}

void HikvisionMetadataMonitor::initMonitorUnsafe()
{
    initEventMonitor();

    for (const auto& eventTypeInternalName: { "BlackList", "WhiteList", "otherlist" })
    {
        if (m_deviceManifest.supportedEventTypeIds.contains(
            m_manifest.eventTypeByInternalName(eventTypeInternalName).id))
        {
            initLprMonitor();
            break;
        }
    }
}

void HikvisionMetadataMonitor::initEventMonitor()
{
    auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
    m_monitorTimer.pleaseStopSync();
    httpClient->bindToAioThread(m_timer.getAioThread());

    httpClient->setOnResponseReceived([this]() { at_monitorResponseReceived(); });
    httpClient->setOnSomeMessageBodyAvailable([this]() { at_monitorSomeBytesAvailable(); });
    httpClient->setOnDone([this]() { reopenMonitorConnection(); });

    m_timeSinceLastOpen.restart();
    httpClient->setTotalReconnectTries(nx::network::http::AsyncClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setResponseReadTimeout(kKeepAliveTimeout);
    httpClient->setMessageBodyReadTimeout(kKeepAliveTimeout);

    m_contentParser = std::make_unique<nx::network::http::MultipartContentParser>();
    m_contentParser->setNextFilter(
        std::make_shared<BytestreamFilter>(m_manifest, this, m_contentParser.get()));

    m_monitorHttpClient = std::move(httpClient);
    m_monitorHttpClient->doGet(m_monitorUrl);
}

void HikvisionMetadataMonitor::initLprMonitor()
{
    auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
    m_lprTimer.pleaseStopSync();
    httpClient->bindToAioThread(m_timer.getAioThread());

    httpClient->setOnDone([this]() { at_LprRequestDone(); });

    httpClient->setTotalReconnectTries(nx::network::http::AsyncClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setResponseReadTimeout(kLprHttpRequestTimeout);
    httpClient->setMessageBodyReadTimeout(kLprHttpRequestTimeout);
    m_lprHttpClient = std::move(httpClient);
    sendLprRequest();
}

void HikvisionMetadataMonitor::sendLprRequest()
{
    static const QString kLprRequest("<AfterTime><picTime>%1</picTime></AfterTime>");
    QByteArray requestBody = kLprRequest.arg(m_fromDateFilter).toLatin1();
    m_lprHttpClient->setRequestBody(
        std::make_unique<nx::network::http::BufferSource>(
            Qn::serializationFormatToHttpContentType(Qn::XmlFormat),
            std::move(requestBody)));
    m_lprHttpClient->doPost(m_lprUrl);

}

void HikvisionMetadataMonitor::at_monitorResponseReceived()
{
    if (!m_monitorHttpClient)
        return;
    const auto response = m_monitorHttpClient->response();
    if (response && response->statusLine.statusCode == nx::network::http::StatusCode::ok)
        m_contentParser->setContentType(m_monitorHttpClient->contentType());
    else
        reopenMonitorConnection();
}

void HikvisionMetadataMonitor::at_LprRequestDone()
{
    if (!m_lprHttpClient)
        return;

    const auto response = m_lprHttpClient->response();
    if (!response || response->statusLine.statusCode != nx::network::http::StatusCode::ok)
    {
        reopenLprConnection();
        return;
    }

    auto hikvisionEvents =
        AttributesParser::parseLprXml(m_lprHttpClient->fetchMessageBodyBuffer(), m_manifest);
    const bool isFirstTime = m_fromDateFilter.isEmpty();
    for (const auto& hikvisionEvent : hikvisionEvents)
    {
        if (!isFirstTime)
            processEvent({ hikvisionEvent });
        m_fromDateFilter = hikvisionEvent.picName;
    }

    m_lprTimer.start(kLprRequestsTimeout, [this]() { sendLprRequest(); });
}

void HikvisionMetadataMonitor::at_monitorSomeBytesAvailable()
{
    if (!m_monitorHttpClient)
        return;
    const auto& buffer = m_monitorHttpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);

    // While processing this empty event HikvisionMetadataMonitor::addExpiredEvents finds
    // expired state-dependent events, which then are sent to server with field `active=false`.
    // This makes new-coming active state-dependent events not to be ignored by server.
    processEvent(EventWithRegions());
}

std::chrono::milliseconds HikvisionMetadataMonitor::reopenDelay() const
{
    const auto elapsed = m_timeSinceLastOpen.elapsed();
    const auto ms = std::max(0LL, (qint64) std::chrono::duration_cast
        <std::chrono::milliseconds>(kMinReopenInterval).count() - elapsed);
    return std::chrono::milliseconds(ms);
}

void HikvisionMetadataMonitor::reopenMonitorConnection()
{
    m_monitorTimer.start(reopenDelay(), [this]() { initEventMonitor(); });
}

void HikvisionMetadataMonitor::reopenLprConnection()
{
    m_lprTimer.start(reopenDelay(), [this]() { initLprMonitor(); });
}

std::vector<HikvisionEvent> HikvisionMetadataMonitor::makeHikvisionEvents(
    const EventWithRegions& eventWithRegions)
{
    std::vector<HikvisionEvent> result;
    if (!eventWithRegions.event.typeId.isEmpty())
    {
        if (m_deviceManifest.supportedEventTypeIds.contains(eventWithRegions.event.typeId))
        {
            // Normal case: supported event received.
            result.push_back(eventWithRegions.event);

            if (eventWithRegions.event.isThermal())
            {
                // This is a thermal event, probably it should generate some more thermal events.
                for (const Region& region: eventWithRegions.regions)
                {
                    if (region.id <= kMaxSupportedRegionCount)
                    {
                        // Add the same event one more time and update it.
                        result.push_back(eventWithRegions.event);
                        HikvisionEvent& lastEvent = result.back();
                        lastEvent.typeId.replace(QString("Any"), QString::number(region.id));
                        lastEvent.region = region;
                    }
                }
            }
        }
        else if (m_receivedUnsupportedEventTypes.count(eventWithRegions.event.typeId) == 0)
        {
            // The event is not supported by the current device agent. We should log it,
            // but only once.
            NX_DEBUG(this, "Unsupported event type notification \"%1\" received"
                " for the Device %2 (%3)",
                eventWithRegions.event.typeId, m_deviceName, m_deviceId);
            m_receivedUnsupportedEventTypes.insert(eventWithRegions.event.typeId);
        }
    }

    for (HikvisionEvent& event: result)
        event.description = buildDescription(m_manifest, event);

    return result;
}

bool HikvisionMetadataMonitor::processEvent(const EventWithRegions& eventWithRegions)
{
    std::vector<HikvisionEvent> result = makeHikvisionEvents(eventWithRegions);

    auto getEventKey =
        [](const HikvisionEvent& event)
        {
            QString result = event.typeId;
            if (event.region)
                result += NX_FMT("_%1", event.region->id);
            if (event.channel)
                result += NX_FMT("_%1", *event.channel);
            return result;
        };

    for (const HikvisionEvent& event: result)
    {
        Hikvision::EventType eventType = m_manifest.eventTypeById(event.typeId);
        if (eventType.flags.testFlag(vms::api::analytics::EventTypeFlag::stateDependent))
        {
            const QString key = getEventKey(event);
            if (event.isActive)
                m_startedEvents[key] = StartedEvent(event);
            else
                m_startedEvents.remove(key);
        }
    }

    addExpiredEvents(&result);

    if (result.empty())
        return true;

    for (const HikvisionEvent& e : result)
        NX_VERBOSE(this, lm("Got event %1, isActive=%2").args(e.caption, e.isActive));

    QnMutexLocker lock(&m_mutex);
    for (const auto& handler: m_handlers)
        handler(result);

    return true;
}

void HikvisionMetadataMonitor::addExpiredEvents(std::vector<HikvisionEvent>* result)
{
    for (auto itr = m_startedEvents.begin(); itr != m_startedEvents.end();)
    {
        if (itr.value().timer.hasExpired(kExpiredEventTimeout))
        {
            auto& event = itr.value().event;
            event.isActive = false;
            event.caption = buildCaption(m_manifest, event);
            event.description = buildDescription(m_manifest, event);
            result->push_back(std::move(itr.value().event));
            itr = m_startedEvents.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
