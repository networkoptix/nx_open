#include "manager.h"

#include <chrono>

#include <QtCore/QString>

#include <nx/utils/std/cppnx.h>
#include <nx/utils/std/future.h>
#include <nx/utils/unused.h>

#include <nx/fusion/serialization/json.h>

#include <nx/api/analytics/device_manifest.h>

#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include "parser.h"
#include "log.h"

#define NX_URL_PRINT NX_PRINT << m_url.host().toStdString() << " : "

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace dw_mtt {

namespace {

static const std::chrono::seconds kSendTimeout(15);
static const std::chrono::seconds kReceiveTimeout(15);
static const std::chrono::seconds kReconnectTimeout(30);

static const std::chrono::milliseconds kMinTimeBetweenEvents = std::chrono::seconds(3);

static const int kBufferCapacity = 16 * 1024; //< Enough for big xmls (according to examples).

nx::sdk::metadata::CommonEvent* createCommonEvent(
    const AnalyticsEventType& event, bool active)
{
    auto commonEvent = new nx::sdk::metadata::CommonEvent();
    commonEvent->setTypeId(
        nx::mediaserver_plugins::utils::fromQnUuidToPluginGuid(event.typeId));
    commonEvent->setDescription(event.name.value.toStdString());
    commonEvent->setIsActive(active);
    commonEvent->setConfidence(1.0);
    commonEvent->setAuxilaryData(event.internalName.toStdString());
    return commonEvent;
}

nx::sdk::metadata::CommonEventMetadataPacket* createCommonEventMetadataPacket(
    const AnalyticsEventType& event, bool active = true)
{
    using namespace std::chrono;

    auto packet = new nx::sdk::metadata::CommonEventMetadataPacket();
    auto commonEvent = createCommonEvent(event, active);
    packet->addEvent(commonEvent);
    packet->setTimestampUsec(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUsec(-1);
    return packet;
}

} // namespace

Manager::Manager(Plugin* plugin,
    const nx::sdk::CameraInfo& cameraInfo,
    const AnalyticsDriverManifest& typedManifest)
{
    m_reconnectTimer.bindToAioThread(m_stopEventTimer.getAioThread());

    m_url = cameraInfo.url;
    m_auth.setUser(cameraInfo.login);
    m_auth.setPassword(cameraInfo.password);
    m_plugin = plugin;

    nx::api::AnalyticsDeviceManifest typedCameraManifest;
    for (const auto& eventType : typedManifest.outputEventTypes)
    {
        if(!eventType.unsupported)
            typedCameraManifest.supportedEventTypes.push_back(eventType.typeId);
    }
    m_cameraManifest = QJson::serialized(typedCameraManifest);

    NX_URL_PRINT << "DW MTT metadata manager created";
}

Manager::~Manager()
{
    stopFetchingMetadata();
    NX_URL_PRINT << "DW MTT metadata manager destroyed";
}

void* Manager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nx::sdk::metadata::IID_CameraManager)
    {
        addRef();
        return static_cast<CameraManager*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

/**
 * The manager has received message from camera and should send corresponding message to saerver.
 */
void Manager::treatMessage(QByteArray message)
{
    NX_URL_PRINT << "Treating notification message " << message.constData();
    const QString internalName = QString::fromLatin1(message);

    auto it = std::find_if(m_eventsToCatch.begin(), m_eventsToCatch.end(),
        [&internalName](ElapsedEvent& event)
    {
        return event.type.internalName == internalName;
    });
    if (it != m_eventsToCatch.cend())
    {
        sendEventStartedPacket(it->type);
        if (it->type.isStateful())
        {
            it->timer.start();
            m_stopEventTimer.start(timeTillCheck(), [this]() { onTimerStopEvent(); });
        }
    }
    else
    {
        NX_URL_PRINT << "Packed with undefined event type received. Uuid = "
            << internalName.toStdString();
    }
}

std::chrono::milliseconds Manager::timeTillCheck() const
{
    std::chrono::milliseconds maxElapsed(0);
    for (const ElapsedEvent& event: m_eventsToCatch)
    {
        if (event.type.isStateful())
            maxElapsed = std::max(maxElapsed, event.timer.elapsedSinceStart());
    }
    auto result = kMinTimeBetweenEvents - maxElapsed;
    result = std::max(result, std::chrono::milliseconds::zero());
    return result;
}

void Manager::sendEventStartedPacket(const AnalyticsEventType& event) const
{
    ++m_packetId;
    auto packet = createCommonEventMetadataPacket(event, /*active*/ true);
    m_handler->handleMetadata(nx::sdk::Error::noError, packet);
    NX_URL_PRINT
        << (event.isStateful() ? "Event [start] " : "Event [pulse] ")
        << "packetId = " << m_packetId << " "
        << event.internalName.toUtf8().constData()
        << " sent to server";
}

void Manager::sendEventStoppedPacket(const AnalyticsEventType& event) const
{
    ++m_packetId;
    auto packet = createCommonEventMetadataPacket(event, /*active*/ false);
    m_handler->handleMetadata(nx::sdk::Error::noError, packet);
    NX_URL_PRINT
        << "Event [stop]  "
        << "packetId = " << m_packetId << " "
        << event.internalName.toUtf8().constData()
        << " sent to server";
}

void Manager::onTimerStopEvent()
{
    for (ElapsedEvent& event: m_eventsToCatch)
    {
        if (event.timer.hasExpiredSinceStart(kMinTimeBetweenEvents))
        {
            event.timer.stop();
            sendEventStoppedPacket(event.type);
        }
    }

    if (isTimerNeeded())
        m_stopEventTimer.start(timeTillCheck(), [this](){ onTimerStopEvent(); });
}

/*
 * We need timer if at least one state-dependent event took place recently (i.e. its timer has been
 * started, but not stopped yet).
 */
bool Manager::isTimerNeeded() const
{
    for (const ElapsedEvent& event: m_eventsToCatch)
    {
        if (event.timer.isStarted())
            return true;
    }
    return false;
}

/*
 * Prepar the list of internal names of events that are to be cought.
 * Only one event from each group (except group 0) is allowed.
 */
QList<QByteArray> Manager::internalNamesToCatch() const
{
    QList<QByteArray> names;
    std::set<int> groups;
    for (const auto& eventToCatch : m_eventsToCatch)
    {
        if (!groups.count(eventToCatch.type.group))
        {
            names.push_back(eventToCatch.type.internalName.toLatin1());
            if (eventToCatch.type.group)
                groups.insert(eventToCatch.type.group);
        }
    }
    return names;
}

void Manager::onConnect(SystemError::ErrorCode code)
{
    if (code != SystemError::noError)
    {
        NX_URL_PRINT << "Failed to connect to DW MTT camera notification server. Next connection attempt in"
            << std::chrono::seconds(kReconnectTimeout).count() << " seconds.";
        m_reconnectTimer.start(kReconnectTimeout, [this]() { reconnectSocket(); });
        return;
    }
    NX_URL_PRINT << "Connection to DW MTT camera notification server established";

    QList<QByteArray> names = internalNamesToCatch();
    const QByteArray subscriptionXml = nx::dw_mtt::makeSubscriptionXml(names);
    const nx::network::http::Request subscriptionRequest = m_cameraController.makeHttpRequest(subscriptionXml);
    m_subscriptionRequest = subscriptionRequest.toString();

    QByteArray eventNames;
    for (const auto& eventName : names)
    {
        eventNames = eventNames + eventName + " ";
    }
    NX_URL_PRINT << "Trying to subscribe events: " << eventNames.toStdString();

    m_buffer.clear();
    m_buffer.reserve(kBufferCapacity);

    m_tcpSocket->sendAsync(
        m_subscriptionRequest,
        [this](SystemError::ErrorCode code, size_t size)
        {
            this->onSendSubscriptionQuery(code, size);
        });
}

void Manager::onSendSubscriptionQuery(SystemError::ErrorCode code, size_t size)
{
    if (code != SystemError::noError || size <= 0)
    {
        NX_URL_PRINT << "Failed to send subscription query. Connection will be reconnected in"
            << std::chrono::seconds(kReconnectTimeout).count() << " seconds.";
        m_reconnectTimer.start(kReconnectTimeout, [this]() { reconnectSocket(); });
        return;
    }

    NX_URL_PRINT << "Subscription query sent successfully. Ready to receive data";
    m_tcpSocket->readSomeAsync(
        &m_buffer,
        [this](SystemError::ErrorCode errorCode, size_t size)
    {
        this->onReceive(errorCode, size);
    });
}

/**
 * Extracts xml-data from m_buffer to QDomDocument. Deletes bytes for the beginning of a m_buffer
 * till the end of xml-data.
 * If m_buffer doesn'd contain a complete xml, does nothing, returns null document.
 */
QDomDocument Manager::getDom()
{
    QDomDocument dom;
    static const QByteArray kXmlBeginning =
        R"(<config version="1.7" xmlns="http://www.ipc.com/ver10">)";
    static const QByteArray kXmlEnding = R"(</config>)";
    const auto xmlEndingIterator =
        std::search(m_buffer.cbegin(), m_buffer.cend(), kXmlEnding.cbegin(), kXmlEnding.cend());

    if (xmlEndingIterator == m_buffer.cend())
    {

        NX_URL_PRINT << "Received message doesn't contain complete xml. "
            << "Waiting more incomming data ...";
        return dom;
    }

    const auto xmlBeginningIterator =
        std::search(m_buffer.cbegin(), m_buffer.cend(),
            kXmlBeginning.cbegin(), kXmlBeginning.cend());

    const int xmlSize = xmlEndingIterator - xmlBeginningIterator + kXmlEnding.size();
    const int dataSize = xmlEndingIterator - m_buffer.cbegin() + kXmlEnding.size();

    if (xmlBeginningIterator == m_buffer.cend())
    {
        NX_URL_PRINT << "Bad message received. " <<
            "Threre is xml ending, but no xml beginning in it";

        m_buffer.remove(0, dataSize);
        return dom;
    }

    const QByteArray xml(&*xmlBeginningIterator, xmlSize);
    dom.setContent(xml);
    m_buffer.remove(0, dataSize);

    static const int kTypicalSubscriptionResponseSize = 523;
    if (dataSize == kTypicalSubscriptionResponseSize)
    {
        int set_a_breakpoint_here_in_you_need_to_catch_a_subscription_response = 0;
    }
    else
    {
        int set_a_breakpoint_here_in_you_need_to_catch_a_notification_message = 0;
    }

    return dom;
}

/*
* New metadata have been received from camera. We try to find there information abount
* event occurred. If so, we treat this event.
*/
void Manager::onReceive(SystemError::ErrorCode code, size_t size)
{
    if (code != SystemError::noError || size == 0) //< connection broken or closed
    {
        NX_URL_PRINT << "Receive failed. Connection broken or closed. Next connection attempt in"
            << std::chrono::seconds(kReconnectTimeout).count() << " seconds.";
        m_reconnectTimer.start(kReconnectTimeout, [this]() { reconnectSocket(); });
        return;
    }
    else
    {
        NX_URL_PRINT << size << "byte(s) received";
    }

    QDomDocument dom = this->getDom();
    if(!dom.isNull())
    {
         QByteArray alarmMessage = getEventName(dom);

        if(!alarmMessage.isEmpty())
            treatMessage(alarmMessage);
    }

    m_tcpSocket->readSomeAsync(
        &m_buffer,
        [this](SystemError::ErrorCode errorCode, size_t size)
    {
        this->onReceive(errorCode, size);
    });
}

void Manager::reconnectSocket()
{
    m_reconnectTimer.pleaseStop(
        [this]()
        {
            using namespace std::chrono;
            m_tcpSocket.reset();
            m_tcpSocket = std::make_unique<nx::network::TCPSocket>();
            m_tcpSocket->setNonBlockingMode(true);
            m_tcpSocket->bindToAioThread(m_reconnectTimer.getAioThread());
            m_tcpSocket->setSendTimeout(duration_cast<milliseconds>(kSendTimeout).count());
            m_tcpSocket->setRecvTimeout(duration_cast<milliseconds>(kReceiveTimeout).count());

            m_tcpSocket->connectAsync(
                m_cameraAddress,
                [this](SystemError::ErrorCode code)
            {
                return this->onConnect(code);
            });
    });
}

nx::sdk::Error Manager::startFetchingMetadata(
    nxpl::NX_GUID* eventTypeList, int eventTypeListSize)
{
    const QByteArray host = m_url.host().toLatin1();
    m_cameraController.setIp(m_url.host().toLatin1());
    m_cameraController.setCredentials(m_auth.user().toLatin1(), m_auth.password().toLatin1());

    for (int i = 0; i < eventTypeListSize; ++i)
    {
        QnUuid id = nx::mediaserver_plugins::utils::fromPluginGuidToQnUuid(eventTypeList[i]);
        const AnalyticsEventType* eventType = m_plugin->eventByUuid(id);
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
        return nx::sdk::Error::networkError;
    }
    NX_URL_PRINT << "DW MTT-camera tcp notification port = "
        << m_cameraController.longPollingPort();

    const  QString kAddressPattern("%1:%2");
    QString ipPort = kAddressPattern.arg(
        m_url.host(), QString::number(m_cameraController.longPollingPort()));

    m_cameraAddress = nx::network::SocketAddress(ipPort);

    reconnectSocket();
    return nx::sdk::Error::noError;
}

nx::sdk::Error Manager::setHandler(
    nx::sdk::metadata::MetadataHandler* handler)
{
    m_handler = handler;
    return nx::sdk::Error::noError;
}

nx::sdk::Error Manager::stopFetchingMetadata()
{
    nx::utils::promise<void> promise;
    m_reconnectTimer.pleaseStop(
        [&]()
        {
            m_stopEventTimer.pleaseStopSync();
            m_tcpSocket.reset();
            m_eventsToCatch.clear();
            promise.set_value();
        });
    promise.get_future().wait();
    return nx::sdk::Error::noError;
}

const char* Manager::capabilitiesManifest(nx::sdk::Error* error)
{
    return m_cameraManifest;
}

void Manager::freeManifest(const char* /*data*/)
{
    // Do nothing. Manifest string is stored in member-variable.
}

void Manager::setDeclaredSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
}

} // namespace dw_mtt
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
