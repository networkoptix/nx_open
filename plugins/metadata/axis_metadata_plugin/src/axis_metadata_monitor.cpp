#include "axis_metadata_monitor.h"

//#include <thread>
#include <chrono>
//#include <iostream>
#include <algorithm>

#include <plugins/plugin_internal_tools.h>
#include <nx/utils/std/cpp14.h>
#include <network/tcp_connection_priv.h>
#include <nx/network/system_socket.h>
#include <api/http_client_pool.h>
#include <nx/kit/debug.h>

#include "axis_metadata_manager.h"
#include "camera_controller.h"

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {

static const std::string kWebServerPath("/axiscam");
static const std::string kActionNamePrefix("NX_ACTION_");
static const std::string kRuleNamePrefix("NX_RULE_");

nx::sdk::metadata::CommonDetectedEvent* createCommonDetectedEvent(
    const SupportedEventEx& axisEvent,
    bool active)
{
    auto event = new nx::sdk::metadata::CommonDetectedEvent();
    event->setEventTypeId(axisEvent.externalTypeId());
    event->setCaption(axisEvent.base().name);
    event->setDescription(axisEvent.base().description);
    event->setIsActive(active);
    event->setConfidence(1.0);
    event->setAuxilaryData(axisEvent.base().fullName());
    return event;
}

nx::sdk::metadata::CommonEventMetadataPacket* createCommonEventMetadataPacket(
    const SupportedEventEx& axisEvent)
{
    using namespace std::chrono;

    auto packet = new nx::sdk::metadata::CommonEventMetadataPacket();
    auto event1 = createCommonDetectedEvent(axisEvent, /*active*/true);
    packet->addEvent(event1);
    auto event2 = createCommonDetectedEvent(axisEvent, /*active*/false);
    packet->setTimestampUsec(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUsec(-1);
    packet->addEvent(event2);
    return packet;
}

class axisHandler : public nx_http::AbstractHttpRequestHandler
{
public:
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler)
    {
        NX_PRINT << "Received from Axis: " << request.requestLine.toString().data();

        const QString kMessage = "?Message=";
        const int kGuidStringLength = 36; //sizeof guid string
        int startIndex = request.toString().indexOf(kMessage);
        QString uuidString = request.toString().mid(startIndex + kMessage.size(), kGuidStringLength);
        QnUuid uuid(uuidString);
        nxpl::NX_GUID guid = nxpt::fromQnUuidToPluginGuid(uuid);

        for (const SupportedEventEx& axisEvent : m_axisEvents)
        {
            if (memcmp(&axisEvent.externalTypeId(), &guid, 16) == 0)
            {
                auto packet = createCommonEventMetadataPacket(axisEvent);
                m_handler->handleMetadata(nx::sdk::Error::noError, packet);
                NX_PRINT << "Event detected and set to server: "
                    << axisEvent.base().fullName();
                completionHandler(nx_http::StatusCode::ok);
                return;
            }
        }
        NX_PRINT << "unknown uuid :(";
    }

    axisHandler(nx::sdk::metadata::AbstractMetadataHandler* handler,
        const QList<SupportedEventEx>& axisEvents)
        :
        m_handler(handler),
        m_axisEvents(axisEvents)
    {
    }

private:
    nx::sdk::metadata::AbstractMetadataHandler* m_handler;
    const QList<SupportedEventEx>& m_axisEvents; //< events are actually stored in manager
};

} // namespace

AxisMetadataMonitor::AxisMetadataMonitor(
    AxisMetadataManager* manager,
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

AxisMetadataMonitor::~AxisMetadataMonitor()
{
    stopMonitoring();
    NX_PRINT << "Dtor :" << this;
}

void AxisMetadataMonitor::addRules(const SocketAddress& localAddress, nxpl::NX_GUID* eventTypeList,
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
            m_manager->axisEvents().cbegin(),
            m_manager->axisEvents().cend(),
            [eventTypeList,i](const SupportedEventEx& event)
            {
                return memcmp(&event.externalTypeId(), &eventTypeList[i],
                    sizeof(nxpl::NX_GUID)) == 0;
            });
        if (it != m_manager->axisEvents().cend())
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

void AxisMetadataMonitor::removeRules()
{
    nx::axis::CameraController cameraController(m_url.host().toLatin1(),
        m_auth.user().toLatin1(), m_auth.password().toLatin1());

    int rulesRemoved = cameraController.removeAllActiveRules(kRuleNamePrefix.c_str());
    int actionsRemoved = cameraController.removeAllActiveActions(kActionNamePrefix.c_str());

    NX_PRINT << "rulesRemoved = " << rulesRemoved << ", actionsRemoved = " << actionsRemoved;
}

HostAddress AxisMetadataMonitor::getLocalIp(const SocketAddress& cameraAddress)
{
    int kCameraResponseTimeoutMs = 5000;
    nx::network::TCPSocket s;
    if (s.connect(cameraAddress, kCameraResponseTimeoutMs))
    {
        return s.getLocalAddress().address;
    }
    NX_WARNING(this, "Network connection to camera is broken.\n"
        "Can't detect local IP address for TCP server.\n"
        "Event monitoring can not be started.");
    return HostAddress();
}

nx::sdk::Error AxisMetadataMonitor::startMonitoring(nxpl::NX_GUID* eventTypeList,
    int eventTypeListSize)
{
    const int kSchemePrefixLength = sizeof("http://") - 1;
    QString str = m_url.toString().remove(0, kSchemePrefixLength);

    SocketAddress cameraAddress(str);
    HostAddress localIp=this->getLocalIp(cameraAddress);

    if (localIp == HostAddress())
    {
        //warning message has been outputed int "getLocalIp" function
        return nx::sdk::Error::networkError;
    }

    SocketAddress localAddress(localIp);
    m_httpServer = new TestHttpServer;
    m_httpServer->bindAndListen(localAddress);

    m_httpServer->registerRequestProcessor<axisHandler>(
        kWebServerPath.c_str(),
        [this]() -> std::unique_ptr<axisHandler>
        {
            return std::make_unique<axisHandler>(this->m_handler, this->m_manager->axisEvents());
        },
        nx_http::kAnyMethod);

    localAddress = m_httpServer->server().address();
    this->addRules(localAddress, eventTypeList, eventTypeListSize);
    return nx::sdk::Error::noError;
}

void AxisMetadataMonitor::stopMonitoring()
{
    if (!m_httpServer)
        return;
    delete m_httpServer;
    m_httpServer = nullptr;
    removeRules();
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx

