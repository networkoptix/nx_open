#include "monitor.h"

#include <chrono>
#include <algorithm>

#include <plugins/plugin_internal_tools.h>
#include <nx/utils/std/cpp14.h>
#include <network/tcp_connection_priv.h>
#include <nx/network/system_socket.h>
#include <api/http_client_pool.h>

#include "manager.h"
#include "log.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

namespace {

static const std::string kWebServerPath("/axiscam");
static const std::string kActionNamePrefix("NX_ACTION_");
static const std::string kRuleNamePrefix("NX_RULE_");

static const std::chrono::milliseconds kMinTimeBetweenEvents = std::chrono::seconds(3);

nx::sdk::metadata::CommonEvent* createCommonEvent(const AnalyticsEventType& event, bool active)
{
    auto commonEvent = new nx::sdk::metadata::CommonEvent();
    commonEvent->setEventTypeId(event.eventTypeIdExternal);
    commonEvent->setCaption(event.name.toStdString());
    commonEvent->setDescription(event.eventName.value.toStdString());
    commonEvent->setIsActive(active);
    commonEvent->setConfidence(1.0);
    commonEvent->setAuxilaryData(event.topic.toStdString());
    return commonEvent;
}

nx::sdk::metadata::CommonEventMetadataPacket* createCommonEventMetadataPacket(
    const AnalyticsEventType& event, bool active)
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

void axisHandler::processRequest(
    nx_http::HttpServerConnection* const connection,
    nx::utils::stree::ResourceContainer authInfo,
    nx_http::Request request,
    nx_http::Response* const response,
    nx_http::RequestProcessedHandler completionHandler)
{
    NX_PRINT << "Received from Axis: " << request.requestLine.toString().data();

    const QString kMessage = "?Message=";
    const int kGuidStringLength = 36; //< Size of guid string.
    int startIndex = request.toString().indexOf(kMessage);
    QString uuidString = request.toString().
        mid(startIndex + kMessage.size(), kGuidStringLength);
    QnUuid uuid(uuidString);

    ElapsedEvents& m_events = m_monitor->eventsToCatch();
    const auto it = std::find_if(m_events.begin(), m_events.end(),
        [&uuid](ElapsedEvent& event) { return event.type.eventTypeId == uuid; });
    if (it != m_events.end())
    {
        m_monitor->sendEventStartedPacket(it->type);
        if (it->type.isStateful())
            it->timer.start();
    }
    else
    {
        NX_PRINT << "Received packed with undefined event type. Uuid = "
            << uuidString.toStdString();
    }
    completionHandler(nx_http::StatusCode::ok);
}

Monitor::Monitor(
    Manager* manager,
    const QUrl& url,
    const QAuthenticator& auth,
    nx::sdk::metadata::MetadataHandler* handler)
    :
    m_manager(manager),
    m_url(url),
    m_endpoint(url.toString() + "/vapix/services"),
    m_auth(auth),
    m_handler(handler),
    m_httpServer(nullptr)
{
}

Monitor::~Monitor()
{
    stopMonitoring();
}

void Monitor::addRules(const SocketAddress& localAddress, nxpl::NX_GUID* eventTypeList,
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
            m_manager->events().outputEventTypes.cbegin(),
            m_manager->events().outputEventTypes.cend(),
            [eventTypeList,i](const AnalyticsEventType& event)
            {
                return memcmp(&event.eventTypeIdExternal, &eventTypeList[i],
                    sizeof(nxpl::NX_GUID)) == 0;
            });
        if (it != m_manager->events().outputEventTypes.cend())
        {
            static int globalCounter = 0;

            NX_PRINT << "Try to add action " << fullPath;
            std::string actionName = kActionNamePrefix + std::to_string(++globalCounter);

            // actionEventName - is a human readable event name, a part of a message that camera
            // will send us. The other part of a message is event guid. actionEventName is slightly
            // formatted to be cognizable in http URL query.
            std::string actionEventName = it->fullName().toStdString();
            std::replace(actionEventName.begin(), actionEventName.end(), '/', '.');
            std::replace(actionEventName.begin(), actionEventName.end(), ':', '_');
            std::string message = std::string(it->eventTypeId.toSimpleString().toLatin1()) +
                std::string(".") + actionEventName;

            int actionId = cameraController.addActiveHttpNotificationAction(
                actionName.c_str(),
                message.c_str(),
                fullPath.c_str());
            if(actionId)
                NX_PRINT << "Action addition succeeded, actionId = " << actionId;
            else
                NX_PRINT << "Action addition failed";

            NX_PRINT << "Try to add rule " << it->fullName().toStdString();
            //event.fullname is something like "tns1:VideoSource/tnsaxis:DayNightVision"
            std::string ruleName = kRuleNamePrefix + std::to_string(globalCounter);
            nx::axis::ActiveRule rule(
                ruleName.c_str(),
                /*enabled*/ true,
                it->fullName().toUtf8().constData(),
                actionId);
            const int ruleId = cameraController.addActiveRule(rule);
            if (actionId)
                NX_PRINT << "Rule addition succeeded, ruleId = " << actionId;
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

HostAddress Monitor::getLocalIp(const SocketAddress& cameraAddress)
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

nx::sdk::Error Monitor::startMonitoring(nxpl::NX_GUID* eventTypeList,
    int eventTypeListSize)
{
    for (int i = 0; i < eventTypeListSize; ++i)
    {
        QnUuid id = nxpt::fromPluginGuidToQnUuid(eventTypeList[i]);
        const AnalyticsEventType* eventType = m_manager->eventByUuid(id);
        if (!eventType)
            NX_PRINT << "Unknown event type. TypeId = " << id.toStdString();
        else
            m_eventsToCatch.emplace_back(*eventType);
    }

    const int kSchemePrefixLength = sizeof("http://") - 1;
    QString str = m_url.toString().remove(0, kSchemePrefixLength);

    SocketAddress cameraAddress(str);
    HostAddress localIp=this->getLocalIp(cameraAddress);

    if (localIp == HostAddress())
    {
        // Warning message is already printed by getLocalIp().
        return nx::sdk::Error::networkError;
    }

    SocketAddress localAddress(localIp);
    m_httpServer = new TestHttpServer;
    m_httpServer->server().bindToAioThread(m_aioTimer.getAioThread());

    m_httpServer->bindAndListen(localAddress);
    m_httpServer->registerRequestProcessor<axisHandler>(
        kWebServerPath.c_str(),
        [this]() -> std::unique_ptr<axisHandler> { return std::make_unique<axisHandler>(this); },
        nx_http::kAnyMethod);

    m_aioTimer.start(kMinTimeBetweenEvents, [this](){ onTimer(); });

    localAddress = m_httpServer->server().address();
    this->addRules(localAddress, eventTypeList, eventTypeListSize);
    return nx::sdk::Error::noError;
}

void Monitor::stopMonitoring()
{
    if (!m_httpServer)
        return;
    m_aioTimer.pleaseStopSync();
    delete m_httpServer;
    m_httpServer = nullptr;
    m_eventsToCatch.clear();
    removeRules();
}

std::chrono::milliseconds Monitor::timeTillCheck() const
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

void Monitor::sendEventStartedPacket(const AnalyticsEventType& event) const
{
    auto packet = createCommonEventMetadataPacket(event, /*active*/ true);
    m_handler->handleMetadata(nx::sdk::Error::noError, packet);
    NX_PRINT
        << (event.isStateful() ? "Event [start] " : "Event [pulse] ")
        << event.fullName().toStdString() << " sent to server";
}

void Monitor::sendEventStoppedPacket(const AnalyticsEventType& event) const
{
    auto packet = createCommonEventMetadataPacket(event, /*active*/ false);
    m_handler->handleMetadata(nx::sdk::Error::noError, packet);
    NX_PRINT << "Event [stop] " << event.fullName().toUtf8().constData()
        << " sent to server";
}
void Monitor::onTimer()
{
    for (ElapsedEvent& event: m_eventsToCatch)
    {
        if(event.timer.hasExpiredSinceStart(kMinTimeBetweenEvents))
        {
            event.timer.stop();
            sendEventStoppedPacket(event.type);
        }
    }
    m_aioTimer.start(timeTillCheck(), [this](){ onTimer(); });
}

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

