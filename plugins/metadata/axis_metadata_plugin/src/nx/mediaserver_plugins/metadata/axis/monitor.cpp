#include "monitor.h"

#include <chrono>
#include <algorithm>

#include <plugins/plugin_internal_tools.h>
#include <nx/utils/std/cpp14.h>
#include <network/tcp_connection_priv.h>
#include <nx/network/system_socket.h>
#include <api/http_client_pool.h>
#include <nx/kit/debug.h>

#include "manager.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

namespace {

static const std::string kWebServerPath("/axiscam");
static const std::string kActionNamePrefix("NX_ACTION_");
static const std::string kRuleNamePrefix("NX_RULE_");

nx::sdk::metadata::CommonDetectedEvent* createCommonDetectedEvent(
    const IdentifiedSupportedEvent& identifiedSupportedEvents,
    bool active)
{
    auto detectedEvent = new nx::sdk::metadata::CommonDetectedEvent();
    detectedEvent->setTypeId(identifiedSupportedEvents.externalTypeId());
    detectedEvent->setCaption(identifiedSupportedEvents.base().name);
    detectedEvent->setDescription(identifiedSupportedEvents.base().description);
    detectedEvent->setIsActive(active);
    detectedEvent->setConfidence(1.0);
    detectedEvent->setAuxilaryData(identifiedSupportedEvents.base().fullName());
    return detectedEvent;
}

nx::sdk::metadata::CommonEventMetadataPacket* createCommonEventMetadataPacket(
    const IdentifiedSupportedEvent& identifiedSupportedEvents)
{
    using namespace std::chrono;

    auto packet = new nx::sdk::metadata::CommonEventMetadataPacket();
    auto detectedEvent1 = createCommonDetectedEvent(identifiedSupportedEvents, /*active*/true);
    packet->addEvent(detectedEvent1);
    auto detectedEvent2 = createCommonDetectedEvent(identifiedSupportedEvents, /*active*/false);
    packet->addEvent(detectedEvent2);
    packet->setTimestampUsec(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUsec(-1);
    return packet;
}

class axisHandler : public nx::network::http::AbstractHttpRequestHandler
{
public:
    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        NX_PRINT << "Received from Axis: " << request.requestLine.toString().data();

        const QString kMessage = "?Message=";
        const int kGuidStringLength = 36; //sizeof guid string
        int startIndex = request.toString().indexOf(kMessage);
        QString uuidString = request.toString().
            mid(startIndex + kMessage.size(), kGuidStringLength);
        QnUuid uuid(uuidString);
        nxpl::NX_GUID guid = nxpt::fromQnUuidToPluginGuid(uuid);

        for (const IdentifiedSupportedEvent& axisEvent: m_identifiedSupportedEvents)
        {
            if (memcmp(&axisEvent.externalTypeId(), &guid, 16) == 0)
            {
                auto packet = createCommonEventMetadataPacket(axisEvent);
                m_handler->handleMetadata(nx::sdk::Error::noError, packet);
                NX_PRINT << "Event detected and sent to server: "
                    << axisEvent.base().fullName();
                completionHandler(nx::network::http::StatusCode::ok);
                return;
            }
        }
        NX_PRINT << "unknown uuid :(";
    }

    axisHandler(nx::sdk::metadata::AbstractMetadataHandler* handler,
        const QList<IdentifiedSupportedEvent>& identifiedSupportedEvents)
        :
        m_handler(handler),
        m_identifiedSupportedEvents(identifiedSupportedEvents)
    {
    }

private:
    nx::sdk::metadata::AbstractMetadataHandler* m_handler;

    // events are actually stored in manager
    const QList<IdentifiedSupportedEvent>& m_identifiedSupportedEvents;
};

} // namespace

Monitor::Monitor(
    Manager* manager,
    const QUrl& url,
    const QAuthenticator& auth,
    nx::sdk::metadata::AbstractMetadataHandler* handler)
    :
    m_manager(manager),
    m_url(url),
    m_endpoint(url.toString() + "/vapix/services"),
    m_auth(auth),
    m_handler(handler),
    m_httpServer(nullptr)
{
    NX_PRINT << "Ctor :" << this;
}

Monitor::~Monitor()
{
    stopMonitoring();
    NX_PRINT << "Dtor :" << this;
}

void Monitor::addRules(const nx::network::SocketAddress& localAddress, nxpl::NX_GUID* eventTypeList,
    int eventTypeListSize)
{
    removeRules();

    nx::axis::CameraController cameraController(m_url.host().toLatin1(),
        m_auth.user().toLatin1(), m_auth.password().toLatin1());

    std::string fullPath =
        std::string("http://") + localAddress.toStdString() + kWebServerPath;

    for (int i = 0; i < eventTypeListSize; ++i)
    {
        const auto it = std::find_if(
            m_manager->identifiedSupportedEvents().cbegin(),
            m_manager->identifiedSupportedEvents().cend(),
            [eventTypeList,i](const IdentifiedSupportedEvent& event)
            {
                return memcmp(&event.externalTypeId(), &eventTypeList[i],
                    sizeof(nxpl::NX_GUID)) == 0;
            });
        if (it != m_manager->identifiedSupportedEvents().cend())
        {
            static int globalCounter = 0;

            NX_PRINT << "Try to add action " << fullPath;
            std::string actionName = kActionNamePrefix + std::to_string(++globalCounter);

            // actionEventName - is a human readable event name, a part of a message that camera
            // will send us. The other part of a message is event guid. actionEventName is slightly
            // formatted to be cognizable in http URL query.
            std::string actionEventName = it->base().fullName();
            std::replace(actionEventName.begin(), actionEventName.end(), '/', '.');
            std::replace(actionEventName.begin(), actionEventName.end(), ':', '_');
            std::string message = std::string(it->internalTypeId().toSimpleString().toLatin1()) +
                std::string(".") + actionEventName;

            int actionId = cameraController.addActiveHttpNotificationAction(
                actionName.c_str(),
                message.c_str(),
                fullPath.c_str());
            if(actionId)
                NX_PRINT << "Action addition succeded, actionId = " << actionId;
            else
                NX_PRINT << "Action addition failed";

            NX_PRINT << "Try to add rule " << it->base().fullName();
            //event.fullname is something like "tns1:VideoSource/tnsaxis:DayNightVision"
            std::string ruleName = kRuleNamePrefix + std::to_string(globalCounter);
            nx::axis::ActiveRule rule(
                ruleName.c_str(),
                /*enabled*/ true,
                it->base().fullName().c_str(),
                actionId);
            const int ruleId = cameraController.addActiveRule(rule);
            if (actionId)
                NX_PRINT << "Rule addition succeded, ruleId = " << actionId;
            else
                NX_PRINT << "Rule addition failed";
        }
    }
}

void Monitor::removeRules()
{
    nx::axis::CameraController cameraController(m_url.host().toLatin1(),
        m_auth.user().toLatin1(), m_auth.password().toLatin1());

    int rulesRemoved = cameraController.removeAllActiveRules(kRuleNamePrefix.c_str());
    int actionsRemoved = cameraController.removeAllActiveActions(kActionNamePrefix.c_str());

    NX_PRINT << "rulesRemoved = " << rulesRemoved << ", actionsRemoved = " << actionsRemoved;
}

nx::network::HostAddress Monitor::getLocalIp(const nx::network::SocketAddress& cameraAddress)
{
    std::chrono::milliseconds kCameraResponseTimeoutMs(5000);
    nx::network::TCPSocket s;
    if (s.connect(cameraAddress, kCameraResponseTimeoutMs))
    {
        return s.getLocalAddress().address;
    }
    NX_WARNING(this, "Network connection to camera is broken.\n"
        "Can't detect local IP address for TCP server.\n"
        "Event monitoring can not be started.");
    return nx::network::HostAddress();
}

nx::sdk::Error Monitor::startMonitoring(nxpl::NX_GUID* eventTypeList,
    int eventTypeListSize)
{
    const int kSchemePrefixLength = sizeof("http://") - 1;
    QString str = m_url.toString().remove(0, kSchemePrefixLength);

    nx::network::SocketAddress cameraAddress(str);
    nx::network::HostAddress localIp = this->getLocalIp(cameraAddress);

    if (localIp == nx::network::HostAddress())
    {
        //warning message has been output int "getLocalIp" function
        return nx::sdk::Error::networkError;
    }

    nx::network::SocketAddress localAddress(localIp);
    m_httpServer = new nx::network::http::TestHttpServer;
    m_httpServer->bindAndListen(localAddress);

    m_httpServer->registerRequestProcessor<axisHandler>(
        kWebServerPath.c_str(),
        [this]() -> std::unique_ptr<axisHandler>
        {
            return std::make_unique<axisHandler>(
                this->m_handler, this->m_manager->identifiedSupportedEvents());
        },
        nx::network::http::kAnyMethod);

    localAddress = m_httpServer->server().address();
    this->addRules(localAddress, eventTypeList, eventTypeListSize);
    return nx::sdk::Error::noError;
}

void Monitor::stopMonitoring()
{
    if (!m_httpServer)
        return;
    delete m_httpServer;
    m_httpServer = nullptr;
    removeRules();
}

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

