#pragma once

#include <map>
#include <vector>
#include <shared_mutex>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/metadata/camera_manager.h>
#include <nx/sdk/metadata/common_event.h>
#include <nx/sdk/metadata/common_event_metadata_packet.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/network/http/test_http_server.h>
#include <common/common_module.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace axis {

class ElapsedTimerThreadSafe
{
    // TODO: change to shared_timed_mutex to shared_mutex when c++17 available
    using mutex_type = std::shared_timed_mutex;
    mutable mutex_type m_mutex;
    QElapsedTimer m_timer;

public:
    void start();
    void stop();

    /** @return 0 if timer is stopped when function is invoked */
    std::chrono::milliseconds elapsedSinceStart() const;

    /** @return false if timer was is stopped when function is invoked */
    bool hasExpiredSinceStart(std::chrono::milliseconds ms) const;

#if 0
    // Further methods may be useful, but their usage was excised out the code after refactoring
    bool isStarted() const;
    std::chrono::milliseconds elapsed() const;
    bool hasExpired(std::chrono::milliseconds ms) const;
#endif
};

/**
 * The purpose of ElapsedEvent is to store information when event of corresponding type happened
 * last time.
 * @note ElapsedEvent is thread-safe.
 * @note ElapsedEvent is non-copyable, non-movable and non-default-constructible.
 */
struct ElapsedEvent
{
public:
    const AnalyticsEventType type;
    ElapsedTimerThreadSafe timer;
    ElapsedEvent(const AnalyticsEventType& analyticsEventType): type(analyticsEventType){}
};
using ElapsedEvents = std::list<ElapsedEvent>;

class Manager;

class Monitor;

class axisHandler: public nx_http::AbstractHttpRequestHandler
{
public:
    axisHandler(Monitor* monitor): m_monitor(monitor) {}

    virtual void processRequest(
        nx_http::HttpServerConnection* const connection,
        nx::utils::stree::ResourceContainer authInfo,
        nx_http::Request request,
        nx_http::Response* const response,
        nx_http::RequestProcessedHandler completionHandler);


private:
    Monitor* m_monitor;
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

    void addRules(const SocketAddress& localAddress, nxpl::NX_GUID* eventTypeList,
        int eventTypeListSize);

    void removeRules();

    HostAddress getLocalIp(const SocketAddress& cameraAddress);

    nx::sdk::Error startMonitoring(nxpl::NX_GUID* eventTypeList, int eventTypeListSize);

    void stopMonitoring();

    std::chrono::milliseconds timeTillCheck() const;

    void sendEventStartedPacket(const AnalyticsEventType& event) const;

    void sendEventStoppedPacket(const AnalyticsEventType& event) const;

    void onTimer();

    ElapsedEvents& eventsToCatch() noexcept { return m_eventsToCatch; }

 private:
    Manager * m_manager;
    const QUrl m_url;
    const QUrl m_endpoint;
    const QAuthenticator m_auth;
    nx::sdk::metadata::MetadataHandler* m_handler;
    TestHttpServer* m_httpServer;
    ElapsedEvents m_eventsToCatch; //< The monitor treats events from this list.
    nx::network::aio::Timer m_aioTimer;
};

} // axis
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
