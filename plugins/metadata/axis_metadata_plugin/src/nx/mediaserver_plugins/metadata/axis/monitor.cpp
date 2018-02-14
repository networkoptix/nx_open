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

static const std::chrono::milliseconds kMinTimeBetweenEvents = std::chrono::seconds(3);

nx::sdk::metadata::CommonEvent* createCommonEvent(const AnalyticsEventType& event, bool active)
{
    auto commonEvent = new nx::sdk::metadata::CommonEvent();
    commonEvent->setTypeId(event.eventTypeIdExternal);
    commonEvent->setCaption(event.name.toUtf8().constData());
    commonEvent->setDescription(event.eventName.value.toUtf8().constData());
    commonEvent->setIsActive(active);
    commonEvent->setConfidence(1.0);
    commonEvent->setAuxilaryData(event.topic.toUtf8().constData());
    return commonEvent;
}

nx::sdk::metadata::CommonEventsMetadataPacket* createCommonEventsMetadataPacket(
    const AnalyticsEventType& event, bool active)
{
    using namespace std::chrono;
    auto packet = new nx::sdk::metadata::CommonEventsMetadataPacket();
    auto commonEvent = createCommonEvent(event, active);
    packet->addItem(commonEvent);
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
    nxpl::NX_GUID guid = nxpt::fromQnUuidToPluginGuid(uuid);

    for (const AnalyticsEventTypeExtended& event: m_events)
    {
        if (memcmp(&event.eventTypeIdExternal, &guid, 16) == 0)
        {
            m_monitor->sendEventStartedPacket(event);

            if (event.isStateful())
            {
                std::lock_guard<std::mutex> lock(m_elapsedTimerMutex);
                event.elapsedTimer.start();
            }
            completionHandler(nx_http::StatusCode::ok);
            return;
        }
    }
    NX_PRINT << "unknown uuid";
}

axisHandler::axisHandler(
    Monitor* monitor,
    const QList<AnalyticsEventTypeExtended>& events,
    std::mutex& elapsedTimerMutex)
    :
    m_monitor(monitor),
    m_events(events),
    m_elapsedTimerMutex(elapsedTimerMutex)
{
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
            m_manager->events().cbegin(),
            m_manager->events().cend(),
            [eventTypeList,i](const AnalyticsEventType& event)
            {
                return memcmp(&event.eventTypeIdExternal, &eventTypeList[i],
                    sizeof(nxpl::NX_GUID)) == 0;
            });
        if (it != m_manager->events().cend())
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

            NX_PRINT << "Try to add rule " << it->fullName().toUtf8().constData();
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
    m_eventsToCatch.resize(eventTypeListSize);
    for (int i = 0; i < eventTypeListSize; ++i)
    {
        m_eventsToCatch[i] = nxpt::fromPluginGuidToQnUuid(eventTypeList[i]);
    }

    const int kSchemePrefixLength = sizeof("http://") - 1;
    QString str = m_url.toString().remove(0, kSchemePrefixLength);

    nx::network::SocketAddress cameraAddress(str);
    nx::network::HostAddress localIp = this->getLocalIp(cameraAddress);

    if (localIp == nx::network::HostAddress())
    {
        // Warning message is already printed by getLocalIp().
        return nx::sdk::Error::networkError;
    }

    nx::network::SocketAddress localAddress(localIp);
    m_httpServer = new nx::network::http::TestHttpServer;
    m_httpServer->server().bindToAioThread(m_timer.getAioThread());

    m_httpServer->bindAndListen(localAddress);
    m_httpServer->registerRequestProcessor<axisHandler>(
        kWebServerPath.c_str(),
        [this]() -> std::unique_ptr<axisHandler>
        {
            return std::make_unique<axisHandler>(
                this, m_manager->events(), this->m_elapsedTimerMutex);
        },
        nx::network::http::kAnyMethod);

    m_timer.start(kMinTimeBetweenEvents, [this](){ onTimer(); });

    localAddress = m_httpServer->server().address();
    this->addRules(localAddress, eventTypeList, eventTypeListSize);
    return nx::sdk::Error::noError;
}

void Monitor::stopMonitoring()
{
    if (!m_httpServer)
        return;
    m_timer.pleaseStopSync();
    delete m_httpServer;
    m_httpServer = nullptr;
    removeRules();
}

std::chrono::milliseconds Monitor::timeTillCheck() const
{
    qint64 maxElapsed = 0;
    for (const QnUuid& id: m_eventsToCatch)
    {
        const AnalyticsEventTypeExtended& event = m_manager->eventByUuid(id);
        std::lock_guard<std::mutex> lock(m_elapsedTimerMutex);
        if (event.elapsedTimer.isValid())
            maxElapsed = std::max(maxElapsed, event.elapsedTimer.elapsed());
    }
    auto result = kMinTimeBetweenEvents - std::chrono::milliseconds(maxElapsed);
    result = std::max(result, std::chrono::milliseconds::zero());
    return result;
}

void Monitor::sendEventStartedPacket(const AnalyticsEventType& event) const
{
    auto packet = createCommonEventMetadataPacket(event, /*active*/ true);
    m_handler->handleMetadata(nx::sdk::Error::noError, packet);
    NX_PRINT
        << (event.isStateful() ? "Event [start] " : "Event [pulse] ")
        << event.fullName().toUtf8().constData()
        << " sent to server";
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
    for (const QnUuid& id : m_eventsToCatch)
    {
        const AnalyticsEventTypeExtended& event = m_manager->eventByUuid(id);

        bool isTimeToSend = false;
        {
            std::lock_guard<std::mutex> lock(m_elapsedTimerMutex);
            isTimeToSend = (event.elapsedTimer.isValid()
                && event.elapsedTimer.hasExpired(kMinTimeBetweenEvents.count()));
        }

        if (isTimeToSend)
        {
            {
                std::lock_guard<std::mutex> lock(m_elapsedTimerMutex);
                event.elapsedTimer.invalidate();
            }
            sendEventStoppedPacket(event);
        }
    }
    m_timer.start(timeTillCheck(), [this](){ onTimer(); });
}

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

