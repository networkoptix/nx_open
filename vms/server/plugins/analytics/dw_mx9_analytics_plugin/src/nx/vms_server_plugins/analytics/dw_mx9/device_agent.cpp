#include "device_agent.h"

#include <chrono>
#include <chrono>

#include <QtCore/QString>

#include <nx/utils/std/cppnx.h>
#include <nx/utils/std/future.h>
#include <nx/utils/scope_guard.h>

#include <nx/network/http/buffer_source.h>

#include <nx/fusion/serialization/json.h>

#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/plugin_diagnostic_event.h>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/helpers/error.h>

#include <network/tcp_connection_processor.h>

#include "parser.h"
#include "log.h"
#include "nx/dw_mx9/literals.h"

#define NX_URL_PRINT NX_PRINT << m_url.host().toStdString() << " : "

using namespace std::literals::chrono_literals;

namespace nx::vms_server_plugins::analytics::dw_mx9 {

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
    std::string description = eventType.name.toStdString();
    if (eventType.flags.testFlag(nx::vms::api::analytics::EventTypeFlag::stateDependent))
        description += (active ? " started" : " stopped");
    eventMetadata->setDescription(description);
    eventMetadata->setIsActive(active);
    eventMetadata->setConfidence(1.0);
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

QByteArray prepareUnsubscribeBody(const QByteArray subscribeResponseBody)
{
    QDomDocument doc;
    doc.setContent(subscribeResponseBody);

    QDomNode configNode = doc.elementsByTagName("config").at(0);
    // QDomNodeList::at(int index) return correct empty QDomNode object if index is out of range.
    if (configNode.isNull())
        return {};

    QDomNode child = configNode.firstChild();
    while (!child.isNull())
    {
        QDomNode nextChild = child.nextSibling();
        if (const QDomElement element = child.toElement();
            !element.isNull() && element.tagName() != "serverAddress")
        {
            configNode.removeChild(child);
        }
        child = nextChild;
    }
    return linesTrimmed(doc.toByteArray());
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

    typedCameraManifest.supportedEventTypeIds =
        typedManifest.eventTypeIdListForModel(deviceInfo->model());

    typedCameraManifest.capabilities.setFlag(
        nx::vms::api::analytics::DeviceAgentManifest::disableStreamSelection);

    m_cameraManifest = QJson::serialized(typedCameraManifest);

    NX_URL_PRINT << "DW Mx9 DeviceAgent created";
}

DeviceAgent::~DeviceAgent()
{
    stopFetchingMetadata();
    NX_URL_PRINT << "DW Mx9 DeviceAgent destroyed";
}

bool DeviceAgent::logHttpRequestResult()
{
    if (m_httpClient->state() == nx::network::http::AsyncClient::State::sFailed)
    {
        NX_URL_PRINT << lm("Http request %1 failed with error code %2")
            .args(m_httpClient->url(), m_httpClient->lastSysErrorCode())
            .toStdString();
        return false;
    }

    const nx::network::http::Response* response = m_httpClient->response();
    const int statusCode = response->statusLine.statusCode;
    if (statusCode != nx::network::http::StatusCode::ok)
    {
        NX_URL_PRINT << lm("Http request %1 failed with status code %2")
            .args(m_httpClient->url(), statusCode).toStdString();
        return false;
    }

    NX_URL_PRINT << lm("Http request %1 succeeded with error code %2")
        .args(m_httpClient->url(), m_httpClient->lastSysErrorCode()).toStdString();

    return true;
}

void DeviceAgent::prepareHttpClient(const QByteArray& messageBody,
    std::unique_ptr<nx::network::AbstractStreamSocket> socket/* = {}*/)
{
    NX_CRITICAL(!m_httpClient);
    m_httpClient = std::make_unique<nx::network::http::AsyncClient>(std::move(socket));
    m_httpClient->setUserName(m_auth.user());
    m_httpClient->setUserPassword(m_auth.password());
    m_httpClient->setSendTimeout(kSendTimeout);
    m_httpClient->setResponseReadTimeout(kReceiveTimeout);
    m_httpClient->setMessageBodyReadTimeout(kReceiveTimeout);
    m_httpClient->setAuthType(nx::network::http::AuthType::authBasic);
    m_httpClient->bindToAioThread(m_reconnectTimer.getAioThread());
    m_httpClient->setRequestBody(
        std::make_unique<nx::network::http::BufferSource>(kXmlContentType, messageBody));
}

nx::utils::Url DeviceAgent::makeUrl(const QString& requestName)
{
    const QString kUrlPattern("http://%1:%2/%3");
    nx::utils::Url url = kUrlPattern.arg(
        m_url.host(), QString::number(m_cameraController.longPollingPort()), requestName);
    return url;
}

void DeviceAgent::makeSubscriptionAsync()
{
    if (m_terminated)
        return;

    QnMutexLocker lock(&m_mutex);

    const nx::utils::Url url = makeUrl("SetSubscribe");
    const QSet<QByteArray> names = internalNamesToCatch();
    const QByteArray messageBody = nx::dw_mx9::buildSubscriptionXml(names);

    prepareHttpClient(messageBody);

    m_httpClient->doPost(url, [this]()
        {
            this->onSubscriptionDone();
        });
    NX_URL_PRINT << "Subscription request started.";
}

void DeviceAgent::makeUnsubscriptionSync(std::unique_ptr<nx::network::AbstractStreamSocket> socket)
{
    NX_ASSERT(m_terminated);
    if (!m_terminated)
        return;

    QnMutexLocker lock(&m_mutex);

    const nx::utils::Url url = makeUrl("SetUnSubscribe");

    if (socket)
    {
        // Recreate httpClient and attach it to `socket`.
        prepareHttpClient(m_unsubscribeBody, std::move(socket));
    }
    else
    {
        NX_CRITICAL(m_httpClient);
        // Use existing httpClient, just set new body.
        m_httpClient->setRequestBody(
            std::make_unique<nx::network::http::BufferSource>(kXmlContentType, m_unsubscribeBody));
    }

    std::promise<void> promise;
    m_httpClient->doPost(url, [&promise](){ promise.set_value(); });
    NX_URL_PRINT << "Unsubscription request started.";
    promise.get_future().wait();

    this->logHttpRequestResult();
    m_httpClient.reset();
}

void DeviceAgent::makeDeferredSubscriptionAsync()
{
    m_httpClient.reset();
    m_tcpSocket.reset();
    m_reconnectTimer.cancelSync();
    m_reconnectTimer.start(kReconnectTimeout, [this]()
        {
            makeSubscriptionAsync();
        });
}

void DeviceAgent::onSubscriptionDone()
{
    if (m_terminated)
        return;

    QnMutexLocker lock(&m_mutex);

    const bool requestIsSuccessful = logHttpRequestResult();
    if (!requestIsSuccessful)
    {
        makeDeferredSubscriptionAsync();
        return;
    }

    const QByteArray xmlData = m_httpClient->fetchMessageBodyBuffer();
    m_unsubscribeBody = prepareUnsubscribeBody(xmlData); //< Will be used in SetUnSubscribe request.

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
        makeDeferredSubscriptionAsync();
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
        makeDeferredSubscriptionAsync();
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
                makeDeferredSubscriptionAsync();
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
 * Prepare the list of internal names of events that are to be caught.
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
        NX_URL_PRINT << "Bad message received. Three is xml ending, but no xml beginning in it";
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
            << "Waiting more incoming data ...";
        return QByteArray();
    }
    QByteArray request(m_buffer.constData(), size);

    m_buffer.remove(0, size);
    m_buffer.reserve(kBufferCapacity);

    // The request may contain leading zeros. They hinder to debug. Let's eliminate them.
    int nonZeroIndex = 0;
    while (nonZeroIndex < request.size() && request.at(nonZeroIndex) == '\0')
        ++nonZeroIndex;

    if (nonZeroIndex)
        request.remove(0, nonZeroIndex);

    return request;
}

void DeviceAgent::setHandler(IDeviceAgent::IHandler* handler)
{
    m_handler = shareToPtr(handler);
}

void DeviceAgent::doSetNeededMetadataTypes(
    Result<void>* outResult, const IMetadataTypes* neededMetadataTypes)
{
    const auto eventTypeIds = neededMetadataTypes->eventTypeIds();
    if (const char* const kMessage = "Event type id list is null";
        !NX_ASSERT(eventTypeIds, kMessage))
    {
        *outResult = error(ErrorCode::internalError, kMessage);
        return;
    }

    stopFetchingMetadata();

    if (eventTypeIds->count() != 0)
        *outResult = startFetchingMetadata(neededMetadataTypes);
}

Result<void> DeviceAgent::startFetchingMetadata(const IMetadataTypes* metadataTypes)
{
    m_terminated = false;

    m_cameraController.setIpPort(m_url.host().toLatin1(), m_url.port());
    m_cameraController.setCredentials(m_auth.user().toLatin1(), m_auth.password().toLatin1());

    // Assuming that the list contains only events, since this plugin does not produce objects.
    const auto eventTypeIdList = metadataTypes->eventTypeIds();
    if (const char* const message = "Event type id list is nullptr";
        !NX_ASSERT(eventTypeIdList, message))
    {
        return error(ErrorCode::internalError, message);
    };

    for (int i = 0; i < eventTypeIdList->count(); ++i)
    {
        const QString id(eventTypeIdList->at(i));
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
        eventNames = eventNames + event.type.internalName.toUtf8() + " ";

    NX_URL_PRINT << "Server demanded to start fetching event(s): " << eventNames.constData();

    NX_URL_PRINT << "Trying to get DW Mx9 camera tcp notification server port.";
    if (!m_cameraController.readPortConfiguration())
    {
        static const char* const kMessage =
            "Failed to get DW Mx9 camera tcp notification server port";
        NX_URL_PRINT << kMessage;
        return error(ErrorCode::networkError, kMessage);
    }
    NX_URL_PRINT << "DW Mx9 camera tcp notification port = "
        << m_cameraController.longPollingPort();

    makeSubscriptionAsync();
    return {};
}

void DeviceAgent::stopFetchingMetadata()
{
    m_terminated = true;

    std::promise<void> promise;
    m_reconnectTimer.pleaseStop(
        [&]()
        {
            m_eventsToCatch.clear();

            if (m_httpClient)
                m_httpClient->pleaseStopSync();
            else if (m_tcpSocket)
                m_tcpSocket->pleaseStopSync();

            promise.set_value();
        });
    promise.get_future().wait();

    if (m_httpClient)
    {
        this->makeUnsubscriptionSync({});
        m_httpClient.reset();
    }
    else if (m_tcpSocket)
    {
        this->makeUnsubscriptionSync(std::move(m_tcpSocket));
    }
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    *outResult = new nx::sdk::String(m_cameraManifest);
}

void DeviceAgent::doSetSettings(
    Result<const ISettingsResponse*>* /*outResult*/, const IStringMap* /*settings*/)
{
    // There are no DeviceAgent settings for this plugin.
}

void DeviceAgent::getPluginSideSettings(
    Result<const ISettingsResponse*>* /*outResult*/) const
{
}

} // namespace nx::vms_server_plugins::analytics::dw_mx9
