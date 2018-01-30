#include "monitor.h"

#include <chrono>
#include <algorithm>

#include <plugins/plugin_internal_tools.h>
#include <nx/utils/std/cpp14.h>
#include <network/tcp_connection_priv.h>
#include <api/http_client_pool.h>
#include <nx/kit/debug.h>

#include "manager.h"
#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace vca {

namespace {

int kReceiveTimeoutSeconds = 30;
int kConnectTimeoutSeconds = 5;

struct EventMessage
{
    std::map<std::string, std::string> parameters;
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
static const std::array<std::string, kEventMessageParameterCount> kEventMessageKeys =
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
static const std::array<std::string, kEventMessageParameterCount> kEventMessageSearchKeys =
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
    const std::string& key)
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
        std::string value = std::string(view.first, view.second);
        eventMessage.parameters.insert(std::make_pair(kEventMessageKeys[i], value));
        cur = view.second;
    }
    return eventMessage;
}

// Remove size bytes from the beginning of a buffer.
void cleanBuffer(QByteArray& buffer, int size)
{
    int capacity = buffer.capacity();
    buffer.remove(0, size);
    // Such remove doesn't decrease capacity in Qt5, but it's not guarantied in future.
    buffer.reserve(capacity);
}

struct Receiver
{
    nx::network::TCPSocket& m_socket;
    QByteArray& m_buffer;
    Manager* m_manager;
    nx::sdk::metadata::AbstractMetadataHandler* m_handler;
    std::vector<QnUuid> m_eventsToCatch;
    int id = 0;
    static const int kPrefixSize = 8;
    static const int kSizeSize = 8;
    static const int kPostfixSize = 12;
    static const int kHeaderSize = kPrefixSize + kSizeSize + kPostfixSize;
    static const int kBufferCapacity = 4096; //< 1024 - small buffer is much better for testing
    static const char kPreamble[kPrefixSize]; //< = "DOOFTEN";

    Receiver(
        nx::network::TCPSocket& socket,
        QByteArray& byteArray,
        Manager* manager,
        nx::sdk::metadata::AbstractMetadataHandler* handler,
        const std::vector<QnUuid>& eventsToCatch
        )
        :
        m_socket(socket),
        m_buffer(byteArray),
        m_manager(manager),
        m_handler(handler),
        m_eventsToCatch(eventsToCatch)
    {
        m_buffer.reserve(kBufferCapacity);
    }

    void StartReading()
    {
        m_socket.readSomeAsync(&m_buffer, *this);
    }

    void operator()(SystemError::ErrorCode, size_t)
    {
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
            if (size == 0 || size > kBufferCapacity)
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
                NX_PRINT << "Message got, type=" << it->second << ".";
            else
                NX_PRINT << "Message with unknown type got.";

            const auto& event = m_manager->getVcaAnalyticsEventTypeByInternalName(it->second.c_str());


            if (std::find(m_eventsToCatch.cbegin(), m_eventsToCatch.cend(), event.eventTypeId)
                    != m_eventsToCatch.cend())
            {
                auto packet = createCommonEventMetadataPacket(event);
                m_handler->handleMetadata(nx::sdk::Error::noError, packet);
                NX_PRINT << "Event " << it->second << " sent to server";
            }

            cleanBuffer(m_buffer, size);
            NX_PRINT << "Message treated, size = " << size << " buffer size = " << m_buffer.size()
                << "\n";
        }// while (m_buffer.size() > 0)
        NX_PRINT << "Buffer processing finished. Iteration id = " << id
            << " buffer size = " << m_buffer.size();

        m_socket.readSomeAsync(&m_buffer, *this);
    }
};

const char Receiver::kPreamble[kPrefixSize] = "DOOFTEN";

}// namespace

Monitor::Monitor(
    Manager* manager,
    const QUrl& url,
    const QAuthenticator& auth,
    nx::sdk::metadata::AbstractMetadataHandler* handler)
    :
    m_manager(manager),
    m_url(url),
    m_auth(auth),
    m_handler(handler)
{
    NX_PRINT << "Ctor :" << this;
}

Monitor::~Monitor()
{
    stopMonitoring();
    NX_PRINT << "Dtor :" << this;
}

nx::sdk::Error Monitor::prepareVca(nx::vca::CameraController& vcaCameraConrtoller)
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

    if (!vcaCameraConrtoller.setHeartbeat(nx::vca::Heartbeat(kReceiveTimeoutSeconds, true)))
    {
        NX_PRINT << "Failed to set vca heartbeat";
    }

    return nx::sdk::Error::noError;
}

nx::sdk::Error Monitor::startMonitoring(nxpl::NX_GUID* eventTypeList,
    int eventTypeListSize)
{
    QString host = m_url.host();
    nx::vca::CameraController vcaCameraConrtoller(
        host.toUtf8().constData(),
        m_auth.user().toUtf8().constData(),
        m_auth.password().toUtf8().constData());

    auto error = prepareVca(vcaCameraConrtoller);
    if (error != nx::sdk::Error::noError)
        return error;

    m_currentEventIds.resize(eventTypeListSize);
    for (int i = 0; i < eventTypeListSize; ++i)
    {
        m_currentEventIds[i] = nxpt::fromPluginGuidToQnUuid(eventTypeList[i]);
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
    std::unique_ptr<nx::network::TCPSocket> tcpSocketGuard(m_tcpSocket);

    m_tcpSocket->setNonBlockingMode(true);

    m_tcpSocket->setRecvTimeout((kReceiveTimeoutSeconds + 2) * 1000);

    if (!m_tcpSocket->connect(vcaAddress, kConnectTimeoutSeconds * 1000))
    {
        NX_PRINT << "Failed to connect to camera tcp notification server";
        return nx::sdk::Error::networkError;
    }

    NX_PRINT << "Connection to camera tcp notification server established";

    tcpSocketGuard.release();
    Receiver R(*m_tcpSocket, m_buffer,
        this->manager(), m_handler, m_currentEventIds);
    R.StartReading();

    return nx::sdk::Error::noError;
}

void Monitor::stopMonitoring()
{
    if(m_tcpSocket)
    {
        m_tcpSocket->pleaseStopSync();
        delete m_tcpSocket;
        m_tcpSocket = nullptr;
    }
}

} // namespace vca
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

