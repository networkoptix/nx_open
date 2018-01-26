#include "metadata_monitor.h"
#include "bytestream_filter.h"

#include <QtCore/QUrlQuery>

#include <chrono>
#include <iostream>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log_main.h>
#include <nx/network/http/buffer_source.h>
#include <nx/fusion/serialization_format.h>
#include "attributes_parser.h"
#include "string_helper.h"
#include <nx/sdk/metadata/abstract_event_metadata_packet.h>

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

static const QString kMonitorUrlTemplate("/ISAPI/Event/notification/alertStream");
static const QString kLprUrlTemplate("/ISAPI/Traffic/channels/1/vehicleDetect/plates");

static const int kDefaultHttpPort = 80;
static const std::chrono::minutes kKeepAliveTimeout(2);
static const std::chrono::seconds kMinReopenInterval(10);
static const std::chrono::seconds kLprRequestsTimeout(2);
static const std::chrono::seconds kExpiredEventTimeout(5);

HikvisionMetadataMonitor::HikvisionMetadataMonitor(
    const Hikvision::DriverManifest& manifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth,
    const std::vector<QnUuid>& eventTypes)
    :
    m_manifest(manifest),
    m_monitorUrl(buildMonitoringUrl(url, eventTypes)),
    m_lprUrl(buildLprUrl(url)),
    m_auth(auth)
{
    m_lprTimer.bindToAioThread(m_monitorTimer.getAioThread());
}

HikvisionMetadataMonitor::~HikvisionMetadataMonitor()
{
    stopMonitoring();
}

void HikvisionMetadataMonitor::startMonitoring()
{
    NX_VERBOSE(this, "Started");
    m_monitorTimer.post([this](){ initMonitorUnsafe(); });
}

void HikvisionMetadataMonitor::stopMonitoring()
{
    utils::promise<void> promise;
    m_monitorTimer.post(
        [this, &promise]()
        {
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
    const std::vector<QnUuid>& eventTypes) const
{
    int channel = std::max(1, QUrlQuery(resourceUrl.query()).queryItemValue("channel").toInt());
    QString eventListIds;
    for (const auto& eventTypeId: eventTypes)
    {
        auto name = m_manifest.eventDescriptorById(eventTypeId).internalName;
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
    initLprMonitor();
}

void HikvisionMetadataMonitor::initEventMonitor()
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
    m_contentParser->setNextFilter(std::make_shared<BytestreamFilter>(m_manifest, this));

    m_monitorHttpClient = std::move(httpClient);
    m_monitorHttpClient->doGet(m_monitorUrl);
}

void HikvisionMetadataMonitor::initLprMonitor()
{
    auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
    m_lprTimer.pleaseStopSync();
    httpClient->bindToAioThread(m_lprTimer.getAioThread());

    httpClient->setOnDone([this]() { at_LprRequestDone(); });

    httpClient->setTotalReconnectTries(nx::network::http::AsyncClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setMessageBodyReadTimeout(kKeepAliveTimeout);
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

    auto hikvisionEvents = AttributesParser::parseLprXml(
        m_lprHttpClient->fetchMessageBodyBuffer(), m_manifest);
    const bool isFirstTime = m_fromDateFilter.isEmpty();
    for (const auto& hikvisionEvent: hikvisionEvents)
    {
        if (!isFirstTime)
            processEvent(hikvisionEvent);
        m_fromDateFilter = hikvisionEvent.picName;
    }

    m_lprTimer.start(kLprRequestsTimeout, [this]() { sendLprRequest(); } );
}

void HikvisionMetadataMonitor::at_monitorSomeBytesAvailable()
{
    if (!m_monitorHttpClient)
        return;
    const auto& buffer = m_monitorHttpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);
}

std::chrono::milliseconds HikvisionMetadataMonitor::reopenDelay() const
{
    const auto elapsed = m_timeSinceLastOpen.elapsed();
    const auto ms = std::max(0LL, (qint64)std::chrono::duration_cast
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

bool HikvisionMetadataMonitor::processEvent(const HikvisionEvent& hikvisionEvent)
{
    std::vector<HikvisionEvent> result;
    if (!hikvisionEvent.typeId.isNull())
        result.push_back(hikvisionEvent);

    auto getEventKey = [](const HikvisionEvent& event)
    {
        QString result = event.typeId.toString();
        if (event.region)
            result += QString::number(*event.region) + lit("_");
        if (event.channel)
            result += QString::number(*event.channel);
        return result;
    };

    auto eventDescriptor = m_manifest.eventDescriptorById(hikvisionEvent.typeId);
    using namespace nx::sdk::metadata;
    if (eventDescriptor.flags.testFlag(Hikvision::EventTypeFlag::stateDependent))
    {
        QString key = getEventKey(hikvisionEvent);
        if (hikvisionEvent.isActive)
            m_startedEvents[key] = StartedEvent(hikvisionEvent);
        else
            m_startedEvents.remove(key);
    }
    addExpiredEvents(result);

    if (result.empty())
        return true;

    QnMutexLocker lock(&m_mutex);
    for (const auto handler: m_handlers)
        handler(result);

    return true;
}

void HikvisionMetadataMonitor::addExpiredEvents(std::vector<HikvisionEvent>& result)
{
    for (auto itr = m_startedEvents.begin(); itr != m_startedEvents.end();)
    {
        if (itr.value().timer.hasExpired(kExpiredEventTimeout))
        {
            auto& event = itr.value().event;
            event.isActive = false;
            event.caption = buildCaption(m_manifest, event);
            event.description = buildDescription(m_manifest, event);
            result.push_back(std::move(itr.value().event));
            itr = m_startedEvents.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
