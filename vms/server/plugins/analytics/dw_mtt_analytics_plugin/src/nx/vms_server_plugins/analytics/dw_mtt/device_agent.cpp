#include "device_agent.h"

#include <chrono>
#include <chrono>

#include <QtCore/QString>

#include <nx/utils/std/cppnx.h>
#include <nx/utils/std/future.h>
#include <nx/utils/unused.h>

#include <nx/network/http/buffer_source.h>

#include <nx/fusion/serialization/json.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/sdk/helpers/string.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>

#include <network/tcp_connection_processor.h>

#include "parser.h"
#include "log.h"

#define NX_URL_PRINT NX_PRINT << m_url.host().toStdString() << " : "

using namespace std::literals::chrono_literals;

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dw_mtt {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

static const std::chrono::seconds kSendTimeout(15);
static const std::chrono::seconds kReceiveTimeout(15);
static const std::chrono::seconds kReconnectTimeout(30);

// Camera may send an image in face detection notification, so we need large buffer
static const int kBufferCapacity = 512 * 1024;

static const QByteArray kXmlContentType = "application/xml";

EventMetadata* createCommonEvent(const EventType& eventType, bool active)
{
    auto eventMetadata = new EventMetadata();
    eventMetadata->setTypeId(eventType.id.toStdString());
    eventMetadata->setDescription(eventType.name.toStdString());
    eventMetadata->setIsActive(active);
    eventMetadata->setConfidence(1.0);
    eventMetadata->setAuxiliaryData(eventType.internalName.toStdString());
    return eventMetadata;
}

EventMetadataPacket* createCommonEventMetadataPacket(const EventType& event, bool active = true)
{
    using namespace std::chrono;

    const auto commonEvent = toPtr(createCommonEvent(event, active));
    auto packet = new EventMetadataPacket();
    packet->addItem(commonEvent.get());
    packet->setTimestampUs(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUs(-1);
    return packet;
}

} // namespace

DeviceAgent::DeviceAgent(
    Engine* engine,
    const IDeviceInfo* deviceInfo,
    const EngineManifest& typedManifest)
    :
    m_engine(engine)
{
    m_url = deviceInfo->url();
    m_auth.setUser(deviceInfo->login());
    m_auth.setPassword(deviceInfo->password());

    nx::vms::api::analytics::DeviceAgentManifest typedCameraManifest;
    for (const auto& eventType: typedManifest.eventTypes)
    {
        typedCameraManifest.supportedEventTypeIds.push_back(eventType.id);
    }
    m_cameraManifest = QJson::serialized(typedCameraManifest);

    NX_URL_PRINT << "DW MTT DeviceAgent created";
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    NX_URL_PRINT << "DW MTT DeviceAgent destroyed";
}

void DeviceAgent::prepareHttpClient()
{
    NX_ASSERT(!m_httpClient);
    m_httpClient = std::make_unique<nx::network::http::AsyncClient>();
    m_httpClient->setUserName(m_auth.user());
    m_httpClient->setUserPassword(m_auth.password());
    m_httpClient->setSendTimeout(kSendTimeout);
    m_httpClient->setResponseReadTimeout(kReceiveTimeout);
    m_httpClient->setMessageBodyReadTimeout(kReceiveTimeout);
    m_httpClient->setAuthType(nx::network::http::AuthType::authBasic);
    m_httpClient->bindToAioThread(m_reconnectTimer.getAioThread());
    m_httpClient->setOnDone([this]() { onSubsctiptionDone(); });
}

nx::utils::Url DeviceAgent::makeUrl(const QString& requestName)
{
    const QString kUrlPattern("http://%1:%2/%3");
    nx::utils::Url url = kUrlPattern.arg(
        m_url.host(), QString::number(m_cameraController.longPollingPort()), requestName);
    return url;
}

void DeviceAgent::makeSubscription()
{
    if (m_terminated)
        return;

    QnMutexLocker lock(&m_mutex);

    prepareHttpClient();

    const nx::utils::Url url = makeUrl("SetSubscribe");
    const QSet<QByteArray> names = internalNamesToCatch();
    const QByteArray messageBody = nx::dw_mtt::makeSubscriptionXml(names);

    m_httpClient->setRequestBody(
        std::make_unique<nx::network::http::BufferSource>(kXmlContentType, messageBody));

    m_httpClient->doPost(url);
    NX_URL_PRINT << "Subscription request is sent to server.";
}

void DeviceAgent::makeDeferredSubscription()
{
    m_httpClient.reset();
    m_tcpSocket.reset();
    m_reconnectTimer.start(kReconnectTimeout, [this]() { makeSubscription(); });
}

void DeviceAgent::onSubsctiptionDone()
{
    if (m_terminated)
        return;

    QnMutexLocker lock(&m_mutex);

    if (m_httpClient->state() == nx::network::http::AsyncClient::State::sFailed)
    {
        NX_URL_PRINT << lm("Http request %1 failed with status code %2")
            .args(m_httpClient->contentLocationUrl(), m_httpClient->lastSysErrorCode())
            .toStdString();
        makeDeferredSubscription();
        return;
    }

    const nx::network::http::Response* response = m_httpClient->response();
    const int statusCode = response->statusLine.statusCode;
    if (statusCode != nx::network::http::StatusCode::ok)
    {
        NX_URL_PRINT << lm("Http request %1 failed with status code %2")
            .args(m_httpClient->contentLocationUrl(), statusCode).toStdString();
        makeDeferredSubscription();
        return;
    }

    NX_URL_PRINT << lm("Http request %1 succeeded with status code %2")
        .args(m_httpClient->contentLocationUrl(), m_httpClient->lastSysErrorCode()).toStdString();

    m_buffer.clear();
    m_buffer.reserve(kBufferCapacity);

    m_tcpSocket = m_httpClient->takeSocket();
    m_httpClient.reset();

    readNextNotificationAsync();
}

void DeviceAgent::readNextNotificationAsync()
{
    NX_ASSERT(m_tcpSocket);

    if (!m_tcpSocket)
    {
        makeDeferredSubscription();
        return;
    }
    m_tcpSocket->readSomeAsync(
        &m_buffer,
        [this](SystemError::ErrorCode errorCode, size_t size)
    {
        this->onReceive(errorCode, size);
    });

}

/*
* New metadata have been received from camera. We try to find there information about
* event occurred. If so, we treat this event.
*/
void DeviceAgent::onReceive(SystemError::ErrorCode code, size_t size)
{
    QnMutexLocker lock(&m_mutex);

    if (code == SystemError::timedOut || code == SystemError::again)
    {
        readNextNotificationAsync();
        return;
    }

    if (code != SystemError::noError || size == 0) //< connection broken or closed
    {
        NX_URL_PRINT << "Receive failed. Connection broken or closed. Next connection attempt in "
            << std::chrono::seconds(kReconnectTimeout).count() << " seconds.";
        makeDeferredSubscription();
        return;
    }

    NX_URL_PRINT << size << " byte(s) received";

    QByteArray httpRequest;
    for (;;)
    {
        httpRequest = extractRequestFromBuffer();
        if (httpRequest.isEmpty())
        {
            if (m_buffer.size() == m_buffer.capacity())
            {
                NX_URL_PRINT << "Reading buffer is full, but no complete message found. "
                    << "Connection will be closed. Next connection attempt in "
                    << std::chrono::seconds(kReconnectTimeout).count() << " seconds.";
                makeDeferredSubscription();
                return;
            }
            else
            {
                break;
            }
        }

        NX_URL_PRINT << httpRequest.toStdString();

        QDomDocument dom = this->createDomFromRequest(httpRequest);
        if (!dom.isNull())
        {
            QList<AlarmPair> alarmPairs = getAlarmPairs(dom);
            if (!alarmPairs.isEmpty())
                treatAlarmPairs(alarmPairs);
        }
    }

    readNextNotificationAsync();
}

/**
 * This DeviceAgent has received the alarm message from the camera and should send the
 * corresponding event messages to the server.
 */
void DeviceAgent::treatAlarmPairs(const QList<AlarmPair>& alarmPairs)
{
    for (const auto& alarmPair: alarmPairs)
    {
        auto it = std::find_if(m_eventsToCatch.begin(), m_eventsToCatch.end(),
            [&alarmPair](ElapsedEvent& event)
        {
            return event.type.alarmName == alarmPair.alarmName;
        });

        if (it != m_eventsToCatch.cend())
        {
            if (alarmPair.value)
                sendEventStartedPacket(it->type);
            else
                sendEventStoppedPacket(it->type);
        }
        else
        {
            NX_URL_PRINT << "A packet with unsubscribed alarm type received. Uuid = "
                << alarmPair.alarmName.toStdString();
        }
    }
}

void DeviceAgent::sendEventStartedPacket(const EventType& event) const
{
    ++m_packetId;
    const auto packet = createCommonEventMetadataPacket(event, /*active*/ true);
    m_handler->handleMetadata(packet);
    NX_URL_PRINT
        << (event.isStateful() ? "Event [start] " : "Event [pulse] ")
        << "packetId = " << m_packetId << " "
        << event.internalName.toUtf8().constData() << "."
        << event.alarmName.toUtf8().constData()
        << " sent to server";
}

void DeviceAgent::sendEventStoppedPacket(const EventType& event) const
{
    ++m_packetId;
    const auto packet = createCommonEventMetadataPacket(event, /*active*/ false);
    m_handler->handleMetadata(packet);
    NX_URL_PRINT
        << "Event [stop]  "
        << "packetId = " << m_packetId << " "
        << event.internalName.toUtf8().constData() << "."
        << event.alarmName.toUtf8().constData()
        << " sent to server";
}

/*
 * Prepar the list of internal names of events that are to be cought.
 * Only one event from each group (except group 0) is allowed.
 */
QSet<QByteArray> DeviceAgent::internalNamesToCatch() const
{
    QSet<QByteArray> names;
    std::set<int> groups;
    for (const auto& eventToCatch: m_eventsToCatch)
    {
        if (!groups.count(eventToCatch.type.group))
        {
            names.insert(eventToCatch.type.internalName.toLatin1());
            if (eventToCatch.type.group)
                groups.insert(eventToCatch.type.group);
        }
    }
    return names;
}

QDomDocument DeviceAgent::createDomFromRequest(const QByteArray& request)
{
    QDomDocument dom;
    static const QByteArray kXmlBeginning =
        R"(<config version="1.7" xmlns="http://www.ipc.com/ver10">)";

    const auto xmlBeginningIterator =
        std::search(request.cbegin(), request.cend(),
            kXmlBeginning.cbegin(), kXmlBeginning.cend());

    if (xmlBeginningIterator == m_buffer.cend())
    {
        NX_URL_PRINT << "Bad message received. " <<
            "Threre is xml ending, but no xml beginning in it";

        return dom;
    }

    const int xmlSize = request.cend() - xmlBeginningIterator;
    const QByteArray xml(&*xmlBeginningIterator, xmlSize);

    dom.setContent(xml);
    return dom;
}

QByteArray DeviceAgent::extractRequestFromBuffer()
{
    if (m_buffer.isEmpty())
        return QByteArray();

    int size = QnTCPConnectionProcessor::isFullMessage(m_buffer);
    if (size <= 0)
    {
        NX_URL_PRINT << "Http request is incomplete. "
            << "Waiting more incomming data ...";
        return QByteArray();
    }
    QByteArray request(m_buffer.constData(), size);

    m_buffer.remove(0, size);
    m_buffer.reserve(kBufferCapacity);

    // The request may contain leading zeros. They hinder to debug. Let's eliminate them.
    size_t nonZeroIndex = 0;
    while (nonZeroIndex < request.size() && request.at(nonZeroIndex) == '\0')
        ++nonZeroIndex;

    if (nonZeroIndex)
        request.remove(0, nonZeroIndex);

    return request;
}

Error DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    handler->addRef();
    m_handler.reset(handler);
    return Error::noError;
}

Error DeviceAgent::setNeededMetadataTypes(const IMetadataTypes* metadataTypes)
{
    if (metadataTypes->eventTypeIds()->count() == 0)
    {
        stopFetchingMetadata();
        return Error::noError;
    }

    return startFetchingMetadata(metadataTypes);
}

Error DeviceAgent::startFetchingMetadata(const IMetadataTypes* metadataTypes)
{
    m_terminated = false;

    const QByteArray host = m_url.host().toLatin1();
    m_cameraController.setIp(m_url.host().toLatin1());
    m_cameraController.setCredentials(m_auth.user().toLatin1(), m_auth.password().toLatin1());

    // Assuming that the list contains only events, since this plugin does not produce objects.
    const auto eventTypeList = metadataTypes->eventTypeIds();
    for (int i = 0; i < eventTypeList->count(); ++i)
    {
        const QString id(eventTypeList->at(i));
        const EventType* eventType = m_engine->eventTypeById(id);
        if (!eventType)
            NX_URL_PRINT << "Unknown event type. TypeId = " << id.toStdString();
        else
        {
            m_eventsToCatch.emplace_back(*eventType);
        }
    }

    QByteArray eventNames;
    for (const auto& event: m_eventsToCatch)
    {
        eventNames = eventNames + event.type.internalName.toUtf8() + " ";
    }
    NX_URL_PRINT << "Server demanded to start fetching event(s): " << eventNames.constData();

    NX_URL_PRINT << "Trying to get DW MTT-camera tcp notification server port.";
    if (!m_cameraController.readPortConfiguration())
    {
        NX_URL_PRINT << "Failed to get DW MTT-camera tcp notification server port.";
        return Error::networkError;
    }
    NX_URL_PRINT << "DW MTT-camera tcp notification port = "
        << m_cameraController.longPollingPort();

    makeSubscription();

    return Error::noError;
}

void DeviceAgent::stopFetchingMetadata()
{
    m_terminated = true;
    nx::utils::promise<void> promise;
    m_reconnectTimer.pleaseStop(
        [&]()
        {
            m_eventsToCatch.clear();
            if (m_httpClient)
            {
                m_httpClient->pleaseStopSync();
                m_httpClient.reset();
            }
            if (m_tcpSocket)
            {
                m_tcpSocket->pleaseStopSync();
                m_tcpSocket.reset();
            }
            promise.set_value();
        });
    promise.get_future().wait();
}

const IString* DeviceAgent::manifest(Error* /*error*/) const
{
    return new nx::sdk::String(m_cameraManifest);
}

void DeviceAgent::setSettings(const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
}

IStringMap* DeviceAgent::pluginSideSettings() const
{
    return nullptr;
}

} // namespace dw_mtt
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
