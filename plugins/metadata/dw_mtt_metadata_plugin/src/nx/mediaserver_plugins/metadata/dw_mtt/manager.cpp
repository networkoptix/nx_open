#include "manager.h"

#include <chrono>

#include <QtCore/QUrl>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/api/analytics/device_manifest.h>
#include <nx/fusion/serialization/json.h>
#include <nx/utils/std/cppnx.h>
#include <nx/utils/unused.h>

#include "log.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace dw_mtt {

namespace {

static const std::chrono::seconds kConnectTimeout(5);
static const std::chrono::seconds kSendTimeout(15);
static const std::chrono::seconds kReceiveTimeout(15);
static const std::chrono::seconds kReconnectTimeout(30);

static const std::chrono::milliseconds kMinTimeBetweenEvents = std::chrono::seconds(3);

nx::sdk::metadata::CommonEvent* createCommonEvent(
    const AnalyticsEventType& event, bool active)
{
    auto commonEvent = new nx::sdk::metadata::CommonEvent();
    commonEvent->setEventTypeId(
        nx::mediaserver_plugins::utils::fromQnUuidToPluginGuid(event.eventTypeId));
    commonEvent->setCaption(event.eventName.value.toStdString());
    commonEvent->setDescription(event.eventName.value.toStdString());
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
            typedCameraManifest.supportedEventTypes.push_back(eventType.eventTypeId);
    }
    m_cameraManifest = QJson::serialized(typedCameraManifest);

    static const int kBufferCapacity = 4096;
    m_buffer.reserve(kBufferCapacity);
    NX_PRINT << "DW MTT metadata manager created";
}

Manager::~Manager()
{
    stopFetchingMetadata();
    NX_PRINT << "DW MTT metadata manager destroyed";
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

/*
 * The manager has received message from camera and should send corresponding message to saerver.
 */
void Manager::treatMessage(QByteArray message)
{
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
        NX_PRINT << "Packed with undefined event type received. Uuid = "
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
    NX_PRINT
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
    NX_PRINT
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
        NX_PRINT << "Failed to connect to camera notification server";
        m_reconnectTimer.start(kReconnectTimeout, [this]() { reconnectSocket(); });
        return;
    }

    QList<QByteArray> names = internalNamesToCatch();
    const QByteArray subscriptionXml = nx::dw_mtt::makeSubscriptionXml(names);
    const nx_http::Request subscriptionRequest = m_cameraController.makeHttpRequest(subscriptionXml);
    m_subscriptionRequest = subscriptionRequest.toString();

    m_tcpSocket->sendAsync(
        m_subscriptionRequest,
        [this](SystemError::ErrorCode code, size_t size)
        {
            this->onSend(code, size);
        });
}

void Manager::onSend(SystemError::ErrorCode code, size_t size)
{
    if (code != SystemError::noError || size <= 0)
    {
        NX_PRINT << "Failed to subscribe to events";
        m_reconnectTimer.start(kReconnectTimeout, [this]() { reconnectSocket(); });
        return;
    }

    NX_PRINT << "Connection to camera tcp notification server established";
    m_tcpSocket->readSomeAsync(
        &m_buffer,
        [this](SystemError::ErrorCode errorCode, size_t size)
    {
        this->onReceive(errorCode, size);
    });
}

/*
* New metadata have been received from camera. We try to find there information abount
* event occurred. If so, we treat this event.
*/
void Manager::onReceive(SystemError::ErrorCode code, size_t size)
{
    if (code != SystemError::noError || size == 0) //< connection broken or closed
    {
        m_reconnectTimer.start(kReconnectTimeout, [this]() { reconnectSocket(); });
        return;
    }

    const QByteArray alarmTagOpen = R"(<smartType type="openAlramObj">)";
    const QByteArray alarmTagClose = R"(</smartType>)";

    auto ce = m_buffer.cend();
    auto it = std::search(m_buffer.cbegin(), m_buffer.cend(), alarmTagOpen.cbegin(), alarmTagOpen.cend());
    if (it == m_buffer.cend())
    {
        m_buffer.remove(0, std::distance(m_buffer.cbegin(), it));
    }
    else
    {
        auto start = it + alarmTagOpen.size();
        auto it2 = std::search(start, m_buffer.cend(), alarmTagClose.cbegin(), alarmTagClose.cend());
        if (it2 != m_buffer.cend())
        {
            QByteArray alarmMessage = m_buffer.mid(start - m_buffer.begin(), it2 - start);
            m_buffer.remove(0, std::distance(m_buffer.cbegin(), it2 + alarmTagClose.size()));
            treatMessage(alarmMessage);
        }
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

            m_tcpSocket.reset();
            m_tcpSocket = std::make_unique<nx::network::TCPSocket>();
            m_tcpSocket->setNonBlockingMode(true);
            m_tcpSocket->bindToAioThread(m_reconnectTimer.getAioThread());
            m_tcpSocket->setSendTimeout(kSendTimeout.count() * 1000);
            m_tcpSocket->setRecvTimeout(kReceiveTimeout.count() * 1000);

            m_tcpSocket->connectAsync(
                m_cameraAddress,
                [this](SystemError::ErrorCode code)
            {
                return this->onConnect(code);
            });
    });
}

nx::sdk::Error Manager::startFetchingMetadata(nx::sdk::metadata::MetadataHandler* handler,
    nxpl::NX_GUID* eventTypeList, int eventTypeListSize)
{
    m_handler = handler;

    const QByteArray host = m_url.host().toLatin1();
    m_cameraController.setIp(m_url.host().toLatin1());
    m_cameraController.setUserPassword(m_auth.user().toLatin1(), m_auth.password().toLatin1());

    for (int i = 0; i < eventTypeListSize; ++i)
    {
        QnUuid id = nx::mediaserver_plugins::utils::fromPluginGuidToQnUuid(eventTypeList[i]);
        const AnalyticsEventType* eventType = m_plugin->eventByUuid(id);
        if (!eventType)
            NX_PRINT << "Unknown event type. TypeId = " << id.toStdString();
        else
        {
            m_eventsToCatch.emplace_back(*eventType);
        }
    }
    if (!m_cameraController.readPortConfiguration())
    {
        NX_PRINT << "Failed to get DW MTT-camera tcp notification server port.";
        return nx::sdk::Error::networkError;
    }

    const  QString kAddressPattern("%1:%2");
    QString ipPort = kAddressPattern.arg(
        m_url.host(), QString::number(m_cameraController.longPollingPort()));

    m_cameraAddress = SocketAddress(ipPort);

    reconnectSocket();
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

} // namespace dw_mtt
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
