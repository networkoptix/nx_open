#include "monitor.h"

#include <chrono>
#include <algorithm>

#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/utils/std/cpp14.h>
#include <nx/network/system_socket.h>

#include <nx/kit/debug.h>

#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::axis {

namespace {

static const std::string kWebServerPath("/axiscam");
static const std::string kActionNamePrefix("NX_ACTION_");
static const std::string kRuleNamePrefix("NX_RULE_");

static const std::chrono::milliseconds kMinTimeBetweenEvents = std::chrono::seconds(3);

nx::sdk::analytics::EventMetadata* createCommonEvent(
    const EventType& eventType, bool active)
{
    auto eventMetadata = new nx::sdk::analytics::EventMetadata();
    eventMetadata->setTypeId(eventType.id.toStdString());
    eventMetadata->setDescription(eventType.name.toStdString());
    eventMetadata->setIsActive(active);
    eventMetadata->setConfidence(1.0);
    eventMetadata->setAuxiliaryData(eventType.topic.toStdString());
    return eventMetadata;
}

nx::sdk::analytics::EventMetadataPacket* createCommonEventsMetadataPacket(
    const EventType& event, bool active)
{
    using namespace std::chrono;
    auto commonEvent = toPtr(createCommonEvent(event, active));
    auto packet = new nx::sdk::analytics::EventMetadataPacket();
    packet->addItem(commonEvent.get());
    packet->setTimestampUs(
        duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
    packet->setDurationUs(-1);
    return packet;
}

} // namespace

#if 0
// Further methods may be useful, but their usage was excised out the code after refactoring
bool ElapsedTimerThreadSafe::isStarted() const
{
    std::shared_lock<mutex_type> lock(m_mutex);
    return m_timer.isValid();
}
std::chrono::milliseconds ElapsedTimerThreadSafe::elapsed() const
{
    std::shared_lock<mutex_type> lock(m_mutex);
    return std::chrono::milliseconds(m_timer.elapsed());
}
bool ElapsedTimerThreadSafe::hasExpired(std::chrono::milliseconds ms) const
{
    std::shared_lock<mutex_type> lock(m_mutex);
    return m_timer.hasExpired(ms.count());
}
#endif

void axisHandler::processRequest(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    const auto& request = requestContext.request;

    NX_PRINT << "Received from Axis: " << request.requestLine.toString().data();
    const QString query = request.requestLine.url.query();

    static const QString kMessageKey = "Message";
    QString id;
    const QStringList queryItems = query.split('&');
    for (const QString& item: queryItems)
    {
        QStringList keyValue = item.split('=');
        if (keyValue.size() > 1 && keyValue[0] == kMessageKey)
        {
            id = keyValue[1];
            break;
        }
    }

    ElapsedEvents& m_events = m_monitor->eventsToCatch();
    const auto it = std::find_if(m_events.begin(), m_events.end(),
        [&id](ElapsedEvent& event) { return event.type.id == id; });
    if (it != m_events.end())
    {
        m_monitor->sendEventStartedPacket(it->type);
        if (it->type.isStateful())
            it->timer.start();
    }
    else
    {
        NX_PRINT << "Received packed with undefined event type. id = " << id.toStdString();
    }
    completionHandler(nx::network::http::StatusCode::ok);
}

Monitor::Monitor(
    DeviceAgent* deviceAgent,
    const QUrl& url,
    const QAuthenticator& auth,
    nx::sdk::analytics::IDeviceAgent::IHandler* handler)
    :
    m_deviceAgent(deviceAgent),
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

void Monitor::addRules(
    const nx::network::SocketAddress& localAddress,
    const nx::sdk::IStringList* eventTypeIds)
{
    removeRules();

    nx::axis::CameraController cameraController(m_url.host().toLatin1(),
        m_auth.user().toLatin1(), m_auth.password().toLatin1());

    std::string fullPath =
        std::string("http://") + localAddress.toStdString() + kWebServerPath;

    for (int i = 0; i < eventTypeIds->count(); ++i)
    {
        const auto it = std::find_if(
            m_deviceAgent->events().eventTypes.cbegin(),
            m_deviceAgent->events().eventTypes.cend(),
            [eventTypeIds, i](const EventType& event)
            {
                return event.id == eventTypeIds->at(i);
            });
        if (it != m_deviceAgent->events().eventTypes.cend())
        {
            static int globalCounter = 0;

            NX_PRINT << "Try to add action " << fullPath;
            std::string actionName = kActionNamePrefix + std::to_string(++globalCounter);

            std::string actionEventTypeName = it->id.toStdString();// fullName().toStdString();

            int actionId = cameraController.addActiveHttpNotificationAction(
                actionName.c_str(),
                actionEventTypeName.c_str(),
                fullPath.c_str());
            if (actionId)
                NX_PRINT << "Action addition succeeded, actionId = " << actionId;
            else
                NX_PRINT << "Action addition failed";

            NX_PRINT << "Try to add rule " << it->id.toStdString();
            // event.fullname is something like "nx.axis.VideoSource-DayNightVision"
            std::string ruleName = kRuleNamePrefix + std::to_string(globalCounter);
            nx::axis::ActiveRule rule(
                ruleName.c_str(),
                /*enabled*/ true,
                it->fullName().toUtf8().constData(),
                actionId);
            const int ruleId = cameraController.addActiveRule(rule);
            if (actionId)
                NX_PRINT << "Rule addition succeeded, ruleId = " << ruleId;
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
    NX_WARNING(this, "Network connection to camera is broken. "
        "Can't detect local IP address for TCP server. "
        "Event monitoring can not be started.");
    return nx::network::HostAddress();
}

nx::sdk::Error Monitor::startMonitoring(
    const nx::sdk::analytics::IMetadataTypes* metadataTypes)
{
    // Assume that the list contains events only, since this plugin produces no objects.
    const auto eventTypeList = metadataTypes->eventTypeIds();
    for (int i = 0; i < eventTypeList->count(); ++i)
    {
        const QString id = eventTypeList->at(i);
        const EventType* eventType = m_deviceAgent->eventTypeById(id);
        if (!eventType)
            NX_PRINT << "Unknown event type id = " << id.toStdString();
        else
            m_eventsToCatch.emplace_back(*eventType);
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
    m_httpServer->server().bindToAioThread(m_aioTimer.getAioThread());

    m_httpServer->bindAndListen(localAddress);
    m_httpServer->registerRequestProcessor<axisHandler>(
        kWebServerPath.c_str(),
        [this]() -> std::unique_ptr<axisHandler> { return std::make_unique<axisHandler>(this); },
        nx::network::http::kAnyMethod);

    m_aioTimer.start(kMinTimeBetweenEvents, [this](){ onTimer(); });

    localAddress = m_httpServer->server().address();
    this->addRules(localAddress, eventTypeList);
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

void Monitor::sendEventStartedPacket(const EventType& event) const
{
    auto packet = createCommonEventsMetadataPacket(event, /*active*/ true);
    m_handler->handleMetadata(packet);
    NX_PRINT
        << (event.isStateful() ? "Event [start] " : "Event [pulse] ")
        << event.fullName().toStdString() << " sent to server";
}

void Monitor::sendEventStoppedPacket(const EventType& event) const
{
    auto packet = createCommonEventsMetadataPacket(event, /*active*/ false);
    m_handler->handleMetadata(packet);
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

} // nx::vms_server_plugins::analytics::axis
