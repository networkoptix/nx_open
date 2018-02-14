#pragma once

#include <map>
#include <vector>
#include <mutex>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/metadata/camera_manager.h>
#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_metadata_packet.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/http/test_http_server.h>
#include <common/common_module.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {


class Manager;

class Monitor;

class axisHandler: public nx::network::http::AbstractHttpRequestHandler
{
public:
    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler);

    axisHandler(
        Monitor* monitor,
        const QList<AnalyticsEventTypeExtended>& events,
        std::mutex& elapsedTimerMutex);

private:
    Monitor* m_monitor;
    const QList<AnalyticsEventTypeExtended>& m_events;
    std::mutex& m_elapsedTimerMutex;
};

class Monitor: public QObject
{
    Q_OBJECT

public:
    using Handler = std::function<void(const QList<AnalyticsEventType>&)>;

    Monitor(
        Manager* manager,
        const QUrl& resourceUrl,
        const QAuthenticator& auth,
        nx::sdk::metadata::MetadataHandler* handler);
    virtual ~Monitor();

    void addRules(const nx::network::SocketAddress& localAddress, nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize);

    void removeRules();

    nx::network::HostAddress getLocalIp(const nx::network::SocketAddress& cameraAddress);

    nx::sdk::Error startMonitoring(nxpl::NX_GUID* eventTypeList, int eventTypeListSize);

    void stopMonitoring();

    std::chrono::milliseconds timeTillCheck() const;

    void sendEventStartedPacket(const AnalyticsEventType& event) const;

    void sendEventStoppedPacket(const AnalyticsEventType& event) const;

    void onTimer();

 private:
    Manager * m_manager;
    const QUrl m_url;
    const QUrl m_endpoint;
    const QAuthenticator m_auth;
    nx::sdk::metadata::MetadataHandler* m_handler;
    std::vector<QnUuid> m_eventsToCatch;
    nx::network::http::TestHttpServer* m_httpServer;
    nx::network::aio::Timer m_timer;
    mutable std::mutex m_elapsedTimerMutex;
};

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
