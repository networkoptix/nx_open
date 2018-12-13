#include "bytestream_filter.h"
#include "metadata_monitor.h"

#include <QtCore/QUrlQuery>

#include <chrono>
//#include <iostream>
//#include <fstream>

#include "attributes_parser.h"
#include "string_helper.h"
#include <nx/fusion/serialization_format.h>
#include <nx/network/http/buffer_source.h>
#include <nx/sdk/analytics/events_metadata_packet.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace dahua {

static const QString kMonitorUrlPath("/cgi-bin/eventManager.cgi");
static const QString kMonitorUrlQueryPattern("action=attach&codes=[%1]&heartbeat=10");

static const std::chrono::minutes kKeepAliveTimeout(2);
static const std::chrono::seconds kMinReopenInterval(10);
static const std::chrono::seconds kExpiredEventTimeout(5);

MetadataMonitor::MetadataMonitor(
    const Dahua::EngineManifest& manifest,
    const nx::vms::api::analytics::DeviceAgentManifest& deviceManifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    const std::vector<QString>& eventTypes)
    :
    m_engineManifest(manifest),
    m_deviceManifest(deviceManifest),
    m_monitorUrl(buildMonitoringUrl(url, eventTypes)),
    m_auth(auth)
{
}

MetadataMonitor::~MetadataMonitor() { stopMonitoring(); }

void MetadataMonitor::startMonitoring()
{
    NX_VERBOSE(this, "Started");
    m_monitorTimer.post([this]() { initEventMonitor(); });
}

void MetadataMonitor::stopMonitoring()
{
    nx::utils::promise<void> promise;
    m_monitorTimer.post([this, &promise]() {
        if (m_monitorHttpClient)
            m_monitorHttpClient->pleaseStopSync();

        m_monitorTimer.pleaseStopSync();
        promise.set_value();
    });

    promise.get_future().wait();
    NX_VERBOSE(this, "Stopped");
}

void MetadataMonitor::addHandler(const QString& handlerId, const Handler& handler)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers[handlerId] = handler;
}

void MetadataMonitor::removeHandler(const QString& handlerId)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.remove(handlerId);
}

void MetadataMonitor::clearHandlers()
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.clear();
}

nx::utils::Url MetadataMonitor::buildMonitoringUrl(
    const nx::utils::Url& resourceUrl,
    const std::vector<QString>& eventTypes) const
{
    NX_ASSERT(!eventTypes.empty());

    nx::utils::Url monitorUrl(resourceUrl);
    monitorUrl.setPath(kMonitorUrlPath);

    QString eventNames;
    for (const auto& eventTypeId: eventTypes)
    {
        const QString name = m_engineManifest.eventTypeDescriptorById(eventTypeId).internalName;
        eventNames = eventNames + name + ',';
    }
    eventNames.chop(1); //< Remove last comma.

    monitorUrl.setQuery(kMonitorUrlQueryPattern.arg(eventNames));
    return monitorUrl;
}

void MetadataMonitor::initEventMonitor()
{
    auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
    m_monitorTimer.pleaseStopSync();
    httpClient->bindToAioThread(m_monitorTimer.getAioThread());

    httpClient->setOnResponseReceived([this]() { at_monitorResponseReceived(); });
    httpClient->setOnSomeMessageBodyAvailable([this]() { at_monitorSomeBytesAvailable(); });
    httpClient->setOnDone([this]() { reopenMonitorConnection(); });

    m_timeSinceLastOpen.restart();
    httpClient->setTotalReconnectTries(nx::network::http::AsyncClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setMessageBodyReadTimeout(kKeepAliveTimeout);

    m_contentParser = std::make_unique<nx::network::http::MultipartContentParser>();
    m_contentParser->setNextFilter(std::make_shared<BytestreamFilter>(m_engineManifest, this));

    m_monitorHttpClient = std::move(httpClient);
    m_monitorHttpClient->doGet(m_monitorUrl);
}

void MetadataMonitor::at_monitorResponseReceived()
{
    if (!m_monitorHttpClient)
        return;
    const auto response = m_monitorHttpClient->response();
    if (response && response->statusLine.statusCode == nx::network::http::StatusCode::ok)
        m_contentParser->setContentType(m_monitorHttpClient->contentType());
    else
        reopenMonitorConnection();
}

void MetadataMonitor::at_monitorSomeBytesAvailable()
{
    if (!m_monitorHttpClient)
        return;
    const auto& buffer = m_monitorHttpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);
}

std::chrono::milliseconds MetadataMonitor::reopenDelay() const
{
    const auto elapsed = m_timeSinceLastOpen.elapsed();
    const auto ms = std::max(0LL, (qint64) std::chrono::duration_cast
        <std::chrono::milliseconds>(kMinReopenInterval).count() - elapsed);
    return std::chrono::milliseconds(ms);
}

void MetadataMonitor::reopenMonitorConnection()
{
    m_monitorTimer.start(reopenDelay(), [this]() { initEventMonitor(); });
}

bool MetadataMonitor::processEvent(const DahuaEvent& event)
{
    using namespace nx::vms::api::analytics;

    std::vector<DahuaEvent> result;
    if (!event.typeId.isEmpty())
        result.push_back(event);

    auto getEventKey =
        [](const DahuaEvent& event)
        {
            QString result = event.typeId;
            if (event.region)
                result += QString::number(*event.region) + lit("_");
            if (event.channel)
                result += QString::number(*event.channel);
            return result;
        };

    auto eventTypeDescriptor = m_engineManifest.eventTypeDescriptorById(event.typeId);
    using namespace nx::sdk::analytics;
    if (eventTypeDescriptor.flags.testFlag(EventTypeFlag::stateDependent))
    {
        const QString key = getEventKey(event);
        if (event.isActive)
            m_startedEvents[key] = StartedEvent(event);
        else
            m_startedEvents.remove(key);
    }
    addExpiredEvents(result);

    if (result.empty())
        return true;

    for (const DahuaEvent& e: result)
        NX_VERBOSE(this, "Got event %1, isActive=%2", e.caption, e.isActive);

    QnMutexLocker lock(&m_mutex);
    for (const auto& handler: m_handlers)
        handler(result);

    return true;
}

void MetadataMonitor::addExpiredEvents(std::vector<DahuaEvent>& result)
{
    for (auto itr = m_startedEvents.begin(); itr != m_startedEvents.end();)
    {
        if (itr.value().timer.hasExpired(kExpiredEventTimeout))
        {
            auto& event = itr.value().event;
            event.isActive = false;
            event.caption = buildCaption(m_engineManifest, event);
            event.description = buildDescription(m_engineManifest, event);
            result.push_back(std::move(itr.value().event));
            itr = m_startedEvents.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

} // namespace dahua
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
