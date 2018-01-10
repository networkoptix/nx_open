#include "axis_metadata_monitor.h"

#include <thread>
#include <chrono>
#include <iostream>

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

/*static*/ const char* const AxisMetadataMonitor::kWebServerPath = "/axiscam";

AxisMetadataMonitor::AxisMetadataMonitor(
    const Axis::DriverManifest& manifest,
    const QUrl& url,
    const QAuthenticator& auth)
    :
    m_manifest(manifest),
    m_url(url),
    m_endpoint(url.toString()+ "/vapix/services"),
    m_auth(auth),
    m_httpServer(nullptr)
{
}

AxisMetadataMonitor::~AxisMetadataMonitor()
{
    stopMonitoring();
}

void AxisMetadataMonitor::setRule(const SocketAddress& localAddress, nxpl::NX_GUID* eventTypeList,
    int eventTypeListSize)
{
    nx::axis::CameraController cameraController(m_url.host().toLatin1(),
        m_auth.user().toLatin1(), m_auth.password().toLatin1());

    std::string fullPath =
        std::string("http://") + localAddress.toStdString() + std::string(kWebServerPath);

    for (int i = 0; i < eventTypeListSize; ++i)
    {
        const auto it = std::find_if(
            m_manager->m_axisEvents.cbegin(),
            m_manager->m_axisEvents.cend(),
            [eventTypeList,i](const AxisEvent& event)
            {
                return memcmp(&event.typeId, &eventTypeList[i], sizeof(event.typeId)) == 0;
            });
        if (it != m_manager->m_axisEvents.cend())
        {
            int actionId = cameraController.addActiveHttpNotificationAction(
                "NX_ACTION_NAME",
                nxpt::fromPluginGuidToQnUuid(it->typeId).toSimpleString().toLatin1(),
                fullPath.c_str());

            //event.fullname has a format like "tns1:VideoSource/tnsaxis:DayNightVision"
            nx::axis::ActiveRule rule("NX_RULE_NAME", /*enabled*/ true,
                it->fullEventName.toLatin1(), actionId);

            const int ruleId = cameraController.addActiveRule(rule);
            QnMutexLocker lock(&m_mutex);
            m_actionIds.push_back(actionId);
            m_ruleIds.push_back(ruleId);
        }
    }
}

class axisHandler: public nx_http::AbstractHttpRequestHandler
{
public:
    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler)
    {
        using namespace std::chrono;

        static int t = 0;
        ++t;
        NX_PRINT << t << ": " << request.toString().data();

        std::shared_ptr<AxisMetadataPlugin::SharedResources> res=
            m_manager->plugin()->sharedResources(m_manager->sharedId());

        const QString kMessage = "?Message=";
        const int kGuidStringLength = 36;
        int startIndex=request.toString().indexOf(kMessage);
        QString uuidString=request.toString().mid(startIndex+kMessage.size(), kGuidStringLength);
        QnUuid uuid(uuidString);
        nxpl::NX_GUID guid = nxpt::fromQnUuidToPluginGuid(uuid);

        for (AxisEvent& axisEvent: m_manager->m_axisEvents)
        {
            if (memcmp(&axisEvent.typeId, &guid, 16) == 0)
            {
                NX_PRINT << t << ": " << axisEvent.fullEventName.toStdString();
                auto packet = new nx::sdk::metadata::CommonEventMetadataPacket();

                auto event1 = new nx::sdk::metadata::CommonDetectedEvent();
                event1->setEventTypeId(axisEvent.typeId);
                event1->setCaption(axisEvent.caption.toStdString());
                event1->setDescription(axisEvent.description.toStdString());
                event1->setIsActive(true);// axisEvent.isActive);
                event1->setConfidence(1.0);
                event1->setAuxilaryData(axisEvent.fullEventName.toStdString());

                packet->addEvent(event1);

                auto event2 = new nx::sdk::metadata::CommonDetectedEvent();
                event2->setEventTypeId(axisEvent.typeId);
                event2->setCaption(axisEvent.caption.toStdString());
                event2->setDescription(axisEvent.description.toStdString());
                event2->setIsActive(false); // TODO: Consider axisEvent.isActive().
                event2->setConfidence(1.0);
                event2->setAuxilaryData(axisEvent.fullEventName.toStdString());

                packet->setTimestampUsec(
                    duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
                packet->setDurationUsec(-1);
                packet->addEvent(event2);

                m_manager->metadataHandler()->handleMetadata(nx::sdk::Error::noError, packet);

                completionHandler(nx_http::StatusCode::ok);
                return;
            }
        }
        NX_PRINT << "unknown uuid :(";
    }

    axisHandler(AxisMetadataManager* manager) :
        m_manager(manager)
    {
    }

private:
    AxisMetadataManager* m_manager;
};

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

nx::sdk::Error AxisMetadataMonitor::startMonitoring(nxpl::NX_GUID* eventTypeList, int eventTypeListSize)
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
        kWebServerPath,
        [&]() -> std::unique_ptr<axisHandler> { return std::make_unique<axisHandler>(m_manager); },
        nx_http::kAnyMethod);

    localAddress = m_httpServer->server().address();
    this->setRule(localAddress, eventTypeList, eventTypeListSize);
    return nx::sdk::Error::noError;
}

void AxisMetadataMonitor::stopMonitoring()
{
    if (!m_httpServer)
        return;
    delete m_httpServer;
    m_httpServer = nullptr;

    std::vector<int> ruleIds;
    std::vector<int> actionIds;
    {
        QnMutexLocker lock(&m_mutex);
        ruleIds = m_ruleIds;
        m_ruleIds.clear();
        actionIds = m_actionIds;
        m_actionIds.clear();
    }

    nx::axis::CameraController cameraController(m_url.host().toLatin1(),
        m_auth.user().toLatin1(), m_auth.password().toLatin1());
    for (int ruleId: ruleIds)
    {
        cameraController.removeActiveRule(ruleId);
    }
    for (int actionId: actionIds)
    {
        cameraController.removeActiveAction(actionId);
    }
}

void AxisMetadataMonitor::setManager(AxisMetadataManager* manager)
{
    QnMutexLocker lock(&m_mutex);
    m_manager = manager;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx

