#pragma once

#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>
#include <QtCore/QElapsedTimer>

#include <nx/network/http/http_async_client.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/utils/url.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dahua {

class MetadataMonitor
{
    using MultipartContentParserPtr = std::unique_ptr<nx::network::http::MultipartContentParser>;

public:
    using Handler = std::function<void(const EventList&)>;

    MetadataMonitor(
        const EngineManifest& parsedEngineManifest,
        const nx::vms::api::analytics::DeviceAgentManifest& parsedDeviceAgentManifest,
        const nx::utils::Url& resourceUrl,
        const QAuthenticator& auth,
        const std::vector<QString>& eventTypeIdList);
    virtual ~MetadataMonitor();

    void startMonitoring();
    void stopMonitoring();

    void addHandler(const QString& handlerId, const Handler& handler);
    void removeHandler(const QString& handlerId);
    void clearHandlers();

    bool processEvent(const Event& event);
private:
    nx::utils::Url buildMonitoringUrl(
        const nx::utils::Url& resourceUrl,
        const std::vector<QString>& eventTypes) const;

    void initEventMonitor();

    void reopenMonitorConnection();

    std::chrono::milliseconds reopenDelay() const;

    void addExpiredEvents(std::vector<Event>& result);
private:
    void at_monitorResponseReceived();
    void at_monitorSomeBytesAvailable();

private:
    const EngineManifest& m_parsedEngineManifest;
    const nx::vms::api::analytics::DeviceAgentManifest& m_parsedDeviceAgentManifest;
    const nx::utils::Url m_monitorUrl;
    const QAuthenticator m_auth;
    nx::network::aio::Timer m_monitorTimer;
    QElapsedTimer m_timeSinceLastOpen;
    std::unique_ptr<nx::network::http::AsyncClient> m_monitorHttpClient;
    MultipartContentParserPtr m_contentParser;

    mutable QnMutex m_mutex;
    QMap<QString, Handler> m_handlers;

    struct StartedEvent
    {
        StartedEvent(const Event& event = Event()) :
            event(event)
        {
            timer.restart();
        }

        Event event;
        nx::utils::ElapsedTimer timer;
    };

    QMap<QString, StartedEvent> m_startedEvents;
};

} // namespace dahua
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
