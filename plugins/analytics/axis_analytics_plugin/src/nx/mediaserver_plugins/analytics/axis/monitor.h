#pragma once

#include <map>
#include <vector>
#include <shared_mutex>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/analytics/device_agent.h>
#include <nx/sdk/analytics/common_event.h>
#include <nx/sdk/analytics/common_metadata_packet.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/elapsed_timer_thread_safe.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace axis {

/**
 * The purpose of ElapsedEvent is to store information when event of corresponding type happened
 * last time.
 * @note ElapsedEvent is thread-safe.
 * @note ElapsedEvent is non-copyable, non-movable and non-default-constructible.
 */
struct ElapsedEvent
{
public:
    const EventType type;
    nx::utils::ElapsedTimerThreadSafe timer;
    ElapsedEvent(const EventType& analyticsEventType): type(analyticsEventType){}
};
using ElapsedEvents = std::list<ElapsedEvent>;

class DeviceAgent;

class Monitor;

class axisHandler: public nx::network::http::AbstractHttpRequestHandler
{
public:
    axisHandler(Monitor* monitor): m_monitor(monitor) {}

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx::network::http::Request request,
        nx::network::http::Response* const response,
        nx::network::http::RequestProcessedHandler completionHandler);


private:
    Monitor* m_monitor;
};

class Monitor: public QObject
{
    Q_OBJECT

public:
    using Handler = std::function<void(const QList<EventType>&)>;

    Monitor(
        DeviceAgent* deviceAgent,
        const QUrl& resourceUrl,
        const QAuthenticator& auth,
        nx::sdk::analytics::MetadataHandler* metadataHandler);
    virtual ~Monitor();

    void addRules(
        const nx::network::SocketAddress& localAddress,
        const nx::sdk::IStringList* eventTypeIds);

    void removeRules();

    nx::network::HostAddress getLocalIp(const nx::network::SocketAddress& cameraAddress);

    nx::sdk::Error startMonitoring(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopMonitoring();

    std::chrono::milliseconds timeTillCheck() const;

    void sendEventStartedPacket(const EventType& event) const;

    void sendEventStoppedPacket(const EventType& event) const;

    void onTimer();

    ElapsedEvents& eventsToCatch() noexcept { return m_eventsToCatch; }

 private:
    DeviceAgent * m_deviceAgent;
    const QUrl m_url;
    const QUrl m_endpoint;
    const QAuthenticator m_auth;
    nx::sdk::analytics::MetadataHandler* m_metadataHandler = nullptr;
    ElapsedEvents m_eventsToCatch; //< The monitor treats events from this list.
    nx::network::http::TestHttpServer* m_httpServer;
    nx::network::aio::Timer m_aioTimer;
    mutable std::mutex m_elapsedTimerMutex;
};

} // namespace axis
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
