#include "manager.h"

#include <chrono>

#include <QtCore/QString>

#define NX_PRINT_PREFIX "[metadata::vca::Manager] "
#include <nx/kit/debug.h>

#include <nx/fusion/serialization/json.h>

#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_metadata_packet.h>

#include <nx/api/analytics/device_manifest.h>

#include <nx/mediaserver_plugins/utils/uuid.h>

#include <nx/utils/std/cppnx.h>

#include "nx/vca/camera_controller.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

namespace {

static const std::chrono::seconds kReconnectTimeout(30);
static const std::chrono::seconds kSendTimeout(15);
static const std::chrono::seconds kReceiveTimeout(15);

const QString heartbeatEventName("heartbeat");

static const std::chrono::milliseconds kMinTimeBetweenEvents = std::chrono::seconds(3);

struct EventMessage
{
    // Translates vca parameters to their values, e.g. "type" -> "md".
    std::map<QByteArray, QByteArray> parameters;
};

nx::sdk::metadata::CommonEvent* createCommonEvent(
    const AnalyticsEventType& eventType, bool active)
{
    auto commonEvent = new nx::sdk::metadata::CommonEvent();
    commonEvent->setTypeId(eventType.id.toStdString());
    commonEvent->setDescription(eventType.name.value.toStdString());
    commonEvent->setIsActive(active);
    commonEvent->setConfidence(1.0);
    commonEvent->setAuxilaryData(eventType.internalName.toStdString());
    return commonEvent;
}

nx::sdk::metadata::CommonEventsMetadataPacket* createCommonEventsMetadataPacket(
    const AnalyticsEventType& event, bool active = true)
{
    using namespace std::chrono;

    auto packet = new nx::sdk::metadata::CommonEventsMetadataPacket();
    const auto commonEvent = createCommonEvent(event, active);
    packet->addItem(commonEvent);
    packet->setTimestampUsec(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUsec(-1);
    return packet;
}

template<class T, size_t N, class Check = std::enable_if_t<!std::is_same<const char*, T>::value>>
std::array<T, N> makeEventSearchKeys(const std::array<T, N>& src)
{
    std::array<T, N> result;
    std::transform(src.begin(), src.end(), result.begin(),
        [](const auto& item)
        {
            static const T prefix("\n");
            static const T postfix("=");
            return prefix + item + postfix;
        });
    return result;
}

static const auto kEventMessageKeys = nx::utils::make_array<QByteArray>(
    "ip", "unitname", "datetime", "dts", "type", "info", "id", "rulesname", "rulesdts");

static const auto kEventMessageSearchKeys = makeEventSearchKeys(kEventMessageKeys);

std::pair<const char*, const char*> findString(const char* messageBegin, const char* messageEnd,
    const QByteArray& key)
{
    const char* keyValue = std::search(messageBegin, messageEnd, key.begin(), key.end());
    if (keyValue == messageEnd)
        return std::make_pair(messageBegin, messageBegin);
    const char* Value = keyValue + key.size();
    const char* ValueEnd = std::find(Value, messageEnd, '\n');
    return std::make_pair(Value, ValueEnd);
}

EventMessage parseMessage(const char* message, int messageLength)
{
    const char* current = message;
    const char* end = message + messageLength;
    EventMessage eventMessage;

    for (int i = 0; i < kEventMessageSearchKeys.size(); ++i)
    {
        auto view = findString(current, end, kEventMessageSearchKeys[i]);
        QByteArray value = QByteArray(view.first, view.second - view.first);
        eventMessage.parameters.insert(std::make_pair(kEventMessageKeys[i], value));
        current = view.second;
    }
    return eventMessage;
}

//** Remove size bytes from the beginning of a buffer while saving current capacity. */
void cleanBuffer(QByteArray& buffer, int size)
{
    const int capacity = buffer.capacity();
    buffer.remove(0, size);
    // Such remove doesn't decrease capacity in Qt5, but it's not guarantied in future.
    buffer.reserve(capacity);
}

bool switchOnEnabledRulesNotification(nx::vca::CameraController& vcaCameraConrtoller)
{
    bool noErrorsOccurred = true;
    for (const auto& rule: vcaCameraConrtoller.suppotedRules())
    {
        if (rule.second.ruleEnabled && !rule.second.tcpServerNotificationEnabled)
        {
            if (!vcaCameraConrtoller.setTcpServerEnabled(/*rule number*/ rule.first, /*on*/ true))
            {
                NX_PRINT << "Failed to switch on tcp server notification for rule "
                    << rule.first << ".";
                noErrorsOccurred = false;
            }
        }
    }
    return noErrorsOccurred;
}

nx::sdk::Error prepare(nx::vca::CameraController& vcaCameraConrtoller)
{
    if (!vcaCameraConrtoller.readSupportedRules2())
    {
        NX_PRINT << "Failed to get VCA-camera analytic rules.";
        return nx::sdk::Error::networkError;
    }

    switchOnEnabledRulesNotification(vcaCameraConrtoller);

    if (std::none_of(
        vcaCameraConrtoller.suppotedRules().cbegin(),
        vcaCameraConrtoller.suppotedRules().cend(),
        [](const std::pair<int, nx::vca::SupportedRule>& rule) { return rule.second.isActive(); }))
    {
        NX_PRINT << "No enabled rules.";
        return nx::sdk::Error::networkError;
    }

    // Switch on VCA-camera heartbeat and set interval slightly less then kReceiveTimeout.
    if (!vcaCameraConrtoller.setHeartbeat(
        nx::vca::Heartbeat(kReceiveTimeout - std::chrono::seconds(2), /*enabled*/ true)))
    {
        NX_PRINT << "Failed to set VCA-camera heartbeat";
        return nx::sdk::Error::networkError;
    }

    return nx::sdk::Error::noError;
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
    for (const auto& eventType: typedManifest.outputEventTypes)
        typedCameraManifest.supportedEventTypes.push_back(eventType.id);
    m_cameraManifest = QJson::serialized(typedCameraManifest);

    static const int kBufferCapacity = 4096;
    m_buffer.reserve(kBufferCapacity);
    NX_PRINT << "VCA metadata manager created";
}

Manager::~Manager()
{
    stopFetchingMetadata();
    NX_PRINT << "VCA metadata manager destroyed";
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

void Manager::treatMessage(int size)
{
    std::replace(m_buffer.data(), m_buffer.data() + size - 1, '\0', '_');
    EventMessage message = parseMessage(m_buffer.data(), size);

    const auto parameterIterator = message.parameters.find("type");
    if (parameterIterator != message.parameters.end())
        NX_PRINT << "Message received. Type = " << parameterIterator->second.constData();
    else
    {
        NX_PRINT << "Message with unknown type received. Type = "
            << parameterIterator->second.constData();
        return;
    }
    const QString internalName = parameterIterator->second;

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
            m_stopEventTimer.start(timeTillCheck(), [this](){ onTimer(); });
        }
    }
    else if (internalName != heartbeatEventName)
    {

        NX_PRINT << "Packed with undefined event type received. Uuid = "
            << internalName.toStdString();
    }

    cleanBuffer(m_buffer, size);
}

void Manager::onReceive(SystemError::ErrorCode code , size_t size)
{
    if (code != SystemError::noError || size == 0) //< connection broken or closed
    {
        NX_PRINT << "Receive failed. Connection broken or closed. Next connection attempt in"
            << std::chrono::seconds(kReconnectTimeout).count() << " seconds.";
        m_reconnectTimer.start(kReconnectTimeout, [this]() { reconnectSocket(); });
        return;
    }

    static const int kPrefixSize = 8;
    static const int kSizeSize = 8;
    static const int kPostfixSize = 12;
    static const int kHeaderSize = kPrefixSize + kSizeSize + kPostfixSize;
    static const char kPreamble[kPrefixSize] = "DOOFTEN";
    static int id = 0;
    ++id;
    NX_PRINT << "\n\n\nBuffer processing started. Iteration id = " << id
        << " buffer size = " << m_buffer.size() << "\n";

    if (m_buffer.isEmpty())
    {
        NX_PRINT << "Connection broken\n";
    }

    while (m_buffer.size() > 0)
    {
        if (m_buffer.size() < kHeaderSize)
        {
            NX_PRINT << "Message header is not complete"
                << ", buffer size = " << m_buffer.size();
            break;
        }
        if (memcmp(m_buffer.data(), kPreamble, kPrefixSize) != 0)
        {
            NX_PRINT << "Corrupted message. Wrong preamble, preamble = "
                << m_buffer.mid(0, kPrefixSize).data()
                << ", buffer size = " << m_buffer.size();
            cleanBuffer(m_buffer, m_buffer.size());
            break;
        }
        const char* const p = m_buffer.data() + kPrefixSize;
        int size = atoi(p);
        if (size == 0 || size > m_buffer.capacity())
        {
            NX_PRINT << "Corrupted message. Wrong message size, message size = " << size
                << ", buffer size = " << m_buffer.size();
            cleanBuffer(m_buffer, m_buffer.size());
            break;
        }
        if (size > m_buffer.size())
        {
            NX_PRINT << "Message is not complete, message size = " << size
                << ", buffer size = " << m_buffer.size();
            break;
        }

        NX_PRINT << "Message ready, size = " << size << " buffer size = " << m_buffer.size();
        treatMessage(size);
        NX_PRINT << "Message treated, size = " << size << " buffer size = " << m_buffer.size()
            << "\n";
    }
    NX_PRINT << "Buffer processing finished. Iteration id = " << id
        << " buffer size = " << m_buffer.size();

    m_tcpSocket->readSomeAsync(
        &m_buffer,
        [this](SystemError::ErrorCode errorCode, size_t size)
        {
            this->onReceive(errorCode, size);
        });
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
    auto packet = createCommonEventsMetadataPacket(event, /*active*/ true);
    m_handler->handleMetadata(nx::sdk::Error::noError, packet);
    NX_PRINT
        << (event.isStateful() ? "Event [start] " : "Event [pulse] ")
        << event.internalName.toUtf8().constData()
        << " sent to server";
}

void Manager::sendEventStoppedPacket(const AnalyticsEventType& event) const
{
    auto packet = createCommonEventsMetadataPacket(event, /*active*/ false);
    m_handler->handleMetadata(nx::sdk::Error::noError, packet);
    NX_PRINT << "Event [stop] " << event.internalName.toUtf8().constData()
        << " sent to server";
}

void Manager::onTimer()
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
        m_stopEventTimer.start(timeTillCheck(), [this](){ onTimer(); });
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

void Manager::onConnect(SystemError::ErrorCode code)
{
    if (code != SystemError::noError)
    {
        NX_PRINT << "Failed to connect to VCA camera notification server. Next connection attempt in"
            << std::chrono::seconds(kReconnectTimeout).count() << " seconds.";
        m_reconnectTimer.start(kReconnectTimeout, [this]() { reconnectSocket(); });
        return;
    }
    NX_PRINT << "Connection to VCA camera notification server established";

    cleanBuffer(m_buffer, m_buffer.size());

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

nx::sdk::Error Manager::setHandler(nx::sdk::metadata::MetadataHandler* handler)
{
    m_handler = handler;
    return nx::sdk::Error::noError;
}

nx::sdk::Error Manager::startFetchingMetadata(const char* const* typeList, int typeListSize)
{
    QString host = m_url.host();
    nx::vca::CameraController vcaCameraConrtoller(host, m_auth.user(), m_auth.password());

    auto error = prepare(vcaCameraConrtoller);
    if (error != nx::sdk::Error::noError)
        return error;

    for (int i = 0; i < typeListSize; ++i)
    {
        QString id = typeList[i];
        const AnalyticsEventType* eventType = m_plugin->eventTypeById(id);
        if (!eventType)
            NX_PRINT << "Unknown event type. TypeId = " << id.toStdString();
        else
            m_eventsToCatch.emplace_back(*eventType);
    }

    QByteArray eventNames;
    for (const auto& event: m_eventsToCatch)
    {
        eventNames = eventNames + event.type.internalName.toUtf8() + " ";
    }
    NX_PRINT << "Server demanded to start fetching event(s): " << eventNames.toStdString();

    if (!vcaCameraConrtoller.readTcpServerPort())
    {
        NX_PRINT << "Failed to get VCA-camera tcp notification server port.";
        return nx::sdk::Error::networkError;
    }

    QString kAddressPattern("%1:%2");
    QString ipPort = kAddressPattern.arg(
        m_url.host(), QString::number(vcaCameraConrtoller.tcpServerPort()));

    m_cameraAddress = nx::network::SocketAddress(ipPort);

    reconnectSocket();

    return nx::sdk::Error::noError;
}

nx::sdk::Error Manager::stopFetchingMetadata()
{
    if (m_tcpSocket)
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
    }
    return nx::sdk::Error::noError;
}

const char* Manager::capabilitiesManifest(nx::sdk::Error* error)
{
    // If camera has no enabled events at the moment, return empty manifest.
    QString host = m_url.host();
    nx::vca::CameraController vcaCameraConrtoller(host, m_auth.user(), m_auth.password());
    if (!vcaCameraConrtoller.readSupportedRulesState())
    {
        NX_PRINT << "Failed to read AVC-camera rules state.";
    }
    for (const auto& rule: vcaCameraConrtoller.suppotedRules())
    {
        if(rule.second.ruleEnabled) //< At least one enabled rule.
            return m_cameraManifest;
    }
    return "";
}

void Manager::freeManifest(const char* /*data*/)
{
    // Do nothing. Manifest string is stored in member-variable.
}

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
