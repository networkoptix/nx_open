#include "manager.h"

#include <chrono>

#include <QtCore/QUrl>

#include <plugins/plugin_internal_tools.h> // nxpt::fromQnUuidToPluginGuid

#include <nx/sdk/metadata/common_detected_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/api/analytics/device_manifest.h>

#include "nx/vca/camera_controller.h"

// Mishas are going to do smth with NX_PRINT/NX_DEBUG.
//#include <nx/utils/log/log.h>
//#define NX_DEBUG_STREAM nx::utils::log::detail::makeStream(nx::utils::log::Level::debug, "VCA")

#include <nx/kit/debug.h>

#include <nx/fusion/serialization/json.h>

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

namespace {

std::chrono::seconds kConnectTimeout(5);
std::chrono::seconds kReceiveTimeout(30);

struct EventMessage
{
    // Translates vca parameters to their values, e.g. "type" -> "md"
    std::map<QByteArray, QByteArray> parameters;
};

nx::sdk::metadata::CommonDetectedEvent* createCommonDetectedEvent(
    const Vca::VcaAnalyticsEventType& event,
    bool active)
{
    auto detectedEvent = new nx::sdk::metadata::CommonDetectedEvent();
    detectedEvent->setEventTypeId(nxpt::fromQnUuidToPluginGuid(event.eventTypeId));
    detectedEvent->setCaption(event.eventName.value.toStdString());
    detectedEvent->setDescription(event.eventName.value.toStdString());
    detectedEvent->setIsActive(active);
    detectedEvent->setConfidence(1.0);
    detectedEvent->setAuxilaryData(event.internalName.toStdString());
    return detectedEvent;
}

nx::sdk::metadata::CommonEventMetadataPacket* createCommonEventMetadataPacket(
    const Vca::VcaAnalyticsEventType& event)
{
    using namespace std::chrono;

    auto packet = new nx::sdk::metadata::CommonEventMetadataPacket();
    auto detectedEvent1 = createCommonDetectedEvent(event, /*active*/true);
    packet->addEvent(detectedEvent1);
    auto detectedEvent2 = createCommonDetectedEvent(event, /*active*/false);
    packet->addEvent(detectedEvent2);
    packet->setTimestampUsec(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUsec(-1);
    return packet;
}

static const int kEventMessageParameterCount = 9;
static const std::array<QByteArray, kEventMessageParameterCount> kEventMessageKeys =
{
    "ip",
    "unitname",
    "datetime",
    "dts",
    "type",
    "info",
    "id",
    "rulesname",
    "rulesdts"
};

// Values of event message parameters may contain names form kEventMessageParameterNames, so to
// avoid search errors we extend key strings with '\n' symbol, '=' symbol is added for convenience.
static const std::array<QByteArray, kEventMessageParameterCount> kEventMessageSearchKeys =
{
    "\nip=",
    "\nunitname=",
    "\ndatetime=",
    "\ndts=",
    "\ntype=",
    "\ninfo=",
    "\nid=",
    "\nrulesname=",
    "\nrulesdts="
};

std::pair<const char*, const char*> findString(const char* msg, const char* end,
    const QByteArray& key)
{
    const char* keyValue = std::search(msg, end, key.begin(), key.end());
    if (keyValue == end)
        return std::make_pair(msg, msg);
    const char* Value = keyValue + key.size();
    const char* ValueEnd = std::find(Value, end, '\n');
    return std::make_pair(Value, ValueEnd);
}

EventMessage parseMessage(const char* msg, int len)
{
    const char* cur = msg;
    const char* end = msg + len;
    EventMessage eventMessage;

    for (int i = 0; i< kEventMessageParameterCount; ++i)
    {
        auto view = findString(cur, end, kEventMessageSearchKeys[i]);
        QByteArray value = QByteArray(view.first, view.second - view.first);
        eventMessage.parameters.insert(std::make_pair(kEventMessageKeys[i], value));
        cur = view.second;
    }
    return eventMessage;
}

// Remove size bytes from the beginning of a buffer while saving current capacity.
void cleanBuffer(QByteArray& buffer, int size)
{
    int capacity = buffer.capacity();
    buffer.remove(0, size);
    // Such remove doesn't decrease capacity in Qt5, but it's not guarantied in future.
    buffer.reserve(capacity);
}

nx::sdk::Error prepare(nx::vca::CameraController& vcaCameraConrtoller)
{
    if (!vcaCameraConrtoller.readSupportedRules2())
    {
        NX_PRINT << "Failed to get VCA analytic rules.";
        return nx::sdk::Error::networkError;
    }

    if (std::none_of(
        vcaCameraConrtoller.suppotedRules().cbegin(),
        vcaCameraConrtoller.suppotedRules().cend(),
        // Generic lambdas not everywhere supported :(
        [](const std::pair<int, nx::vca::SupportedRule>& rule){ return rule.second.ruleEnabled; }))
    {
        NX_PRINT << "No enabled rules.";
        return nx::sdk::Error::networkError;
    }

    bool haveActiveRules = false;
    for (const auto& rule : vcaCameraConrtoller.suppotedRules())
    {
        if (rule.second.ruleEnabled)
        {
            if (rule.second.tcpServerNotificationEnabled)
            {
                haveActiveRules = true;
            }
            else
            {
                bool switchedOn = vcaCameraConrtoller.setTcpServerEnabled(rule.first, true);
                if (!switchedOn)
                {
                    NX_PRINT << "Failed to switch on tcp server notification for rule"
                        << rule.first;
                }
            }

        }
    }
    if (!haveActiveRules)
    {
        NX_PRINT << "Failed to activate at least one rule.";
        return nx::sdk::Error::networkError;
    }

    if (!vcaCameraConrtoller.setHeartbeat(nx::vca::Heartbeat(kReceiveTimeout.count(), true)))
    {
        NX_PRINT << "Failed to set vca heartbeat";
    }

    return nx::sdk::Error::noError;
}

}// namespace

Manager::Manager(Plugin* plugin,
    const nx::sdk::ResourceInfo& resourceInfo,
    const Vca::VcaAnalyticsDriverManifest& typedManifest)
{
    m_url = resourceInfo.url;
    m_auth.setUser(resourceInfo.login);
    m_auth.setPassword(resourceInfo.password);
    m_plugin = plugin;

    nx::api::AnalyticsDeviceManifest typedCameraManifest;
    for (const auto& eventType: typedManifest.outputEventTypes)
        typedCameraManifest.supportedEventTypes.push_back(eventType.eventTypeId);
    m_cameraManifest = QJson::serialized(typedCameraManifest);

    static const int kBufferCapacity = 4096;
    m_buffer.reserve(kBufferCapacity);
    NX_PRINT << "Manager created " << this;
}

Manager::~Manager()
{
    stopFetchingMetadata();
    NX_PRINT << "Manager destroyed " << this;
}

void* Manager::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == nx::sdk::metadata::IID_MetadataManager)
    {
        addRef();
        return static_cast<AbstractMetadataManager*>(this);
    }
    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

void Manager::onReceive(SystemError::ErrorCode, size_t)
{
    static const int kPrefixSize = 8;
    static const int kSizeSize = 8;
    static const int kPostfixSize = 12;
    static const int kHeaderSize = kPrefixSize + kSizeSize + kPostfixSize;
    static const char kPreamble[kPrefixSize] = "DOOFTEN";
    static int id = 0;

    std::cout << "\n\n\n";
    NX_PRINT << "Buffer processing started. Iteration id = " << ++id
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
            // Corrupted message. Throw all received data away and read new.
            NX_PRINT << "Wrong preamble = " << m_buffer.mid(0, kPrefixSize).data()
                << ", buffer size = " << m_buffer.size();
            cleanBuffer(m_buffer, m_buffer.size());
            break;
        }
        const char* p = m_buffer.data() + kPrefixSize;
        int size = atoi(p);
        if (size == 0 || size > m_buffer.capacity())
        {
            // Corrupted message. Throw all received data away and read new.
            NX_PRINT << "Wrong message size, message size = " << size
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

        std::replace(m_buffer.data(), m_buffer.data() + size - 1, '\0', '_');
        EventMessage message = parseMessage(m_buffer.data(), size);

        auto it = message.parameters.find("type");
        if (it != message.parameters.end())
            NX_PRINT << "Message got, type=" << it->second.constData() << ".";
        else
            NX_PRINT << "Message with unknown type got.";

        const auto& event = m_plugin->eventByInternalName(it->second);


        if (std::find(m_eventsToCatch.cbegin(), m_eventsToCatch.cend(), event.eventTypeId)
            != m_eventsToCatch.cend())
        {
            auto packet = createCommonEventMetadataPacket(event);
            m_handler->handleMetadata(nx::sdk::Error::noError, packet);
            NX_PRINT << "Event " << it->second.constData() << " sent to server";
        }

        cleanBuffer(m_buffer, size);
        NX_PRINT << "Message treated, size = " << size << " buffer size = " << m_buffer.size()
            << "\n";
    }// while (m_buffer.size() > 0)
    NX_PRINT << "Buffer processing finished. Iteration id = " << id
        << " buffer size = " << m_buffer.size();

    this->m_tcpSocket->readSomeAsync(
        &m_buffer,
        [this](SystemError::ErrorCode errorCode, size_t size)
        {
            this->onReceive(errorCode, size);
        });
}

nx::sdk::Error Manager::startFetchingMetadata(nx::sdk::metadata::AbstractMetadataHandler* handler,
    nxpl::NX_GUID* eventTypeList, int eventTypeListSize)
{
    QString host = m_url.host();
    nx::vca::CameraController vcaCameraConrtoller(
        host.toUtf8().constData(),
        m_auth.user().toUtf8().constData(),
        m_auth.password().toUtf8().constData());

    auto error = prepare(vcaCameraConrtoller);
    if (error != nx::sdk::Error::noError)
        return error;

    m_eventsToCatch.resize(eventTypeListSize);
    for (int i = 0; i < eventTypeListSize; ++i)
    {
        m_eventsToCatch[i] = nxpt::fromPluginGuidToQnUuid(eventTypeList[i]);
    }

    if (!vcaCameraConrtoller.readTcpServerPort())
    {
        NX_PRINT << "Failed to get VCA tcp notification server port.";
        return nx::sdk::Error::networkError;
    }

    QString kAddressPattern("%1:%2");
    QString ipPort = kAddressPattern.arg(
        m_url.host(), QString::number(vcaCameraConrtoller.tcpServerPort()));

    SocketAddress vcaAddress(ipPort);
    m_tcpSocket = new nx::network::TCPSocket;
    m_tcpSocket->setNonBlockingMode(true);
    m_tcpSocket->setRecvTimeout((kReceiveTimeout.count() + 2) * 1000);
    if (!m_tcpSocket->connect(vcaAddress, kConnectTimeout.count() * 1000))
    {
        NX_PRINT << "Failed to connect to camera tcp notification server";
        return nx::sdk::Error::networkError;
    }
    NX_PRINT << "Connection to camera tcp notification server established";

    m_handler = handler;
    m_tcpSocket->readSomeAsync(
        &m_buffer,
        [this](SystemError::ErrorCode errorCode, size_t size)
        {
            this->onReceive(errorCode, size);
        });

    return nx::sdk::Error::noError;
}

nx::sdk::Error Manager::stopFetchingMetadata()
{
    if (m_tcpSocket)
    {
        m_tcpSocket->pleaseStopSync();
        delete m_tcpSocket;
        m_tcpSocket = nullptr;
    }
    return nx::sdk::Error::noError;
}

const char* Manager::capabilitiesManifest(nx::sdk::Error* error)
{
    return m_cameraManifest;
}

void Manager::freeManifest(const char* data)
{
    // Do nothing. Memory allocated for Manifests is stored in m_givenManifests list and will be
    // released in Manager's destructor.
}

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
