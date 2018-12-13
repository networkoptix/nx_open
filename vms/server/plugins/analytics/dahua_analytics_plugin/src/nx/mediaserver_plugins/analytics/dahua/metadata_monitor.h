#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <map>
#include <vector>

#include "common.h"

#include <nx/network/http/http_async_client.h>
#include <nx/network/http/multipart_content_parser.h>
#include <QtCore/QElapsedTimer>
#include <nx/utils/url.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace dahua {

class MetadataMonitor
{
    using MetadataType = QString;
    using MetadataValue = QString;
    using MultipartContentParserPtr = std::unique_ptr<nx::network::http::MultipartContentParser>;

public:
    using Handler = std::function<void(const DahuaEventList&)>;

    MetadataMonitor(
        const Dahua::EngineManifest& manifest,
        const nx::vms::api::analytics::DeviceAgentManifest& deviceManifest,
        const nx::utils::Url& resourceUrl,
        const QAuthenticator& auth,
        const std::vector<QString>& eventTypes);
    virtual ~MetadataMonitor();

    void startMonitoring();
    void stopMonitoring();

    void addHandler(const QString& handlerId, const Handler& handler);
    void removeHandler(const QString& handlerId);
    void clearHandlers();

    bool processEvent(const DahuaEvent& event);
private:
    nx::utils::Url buildMonitoringUrl(
        const nx::utils::Url& resourceUrl,
        const std::vector<QString>& eventTypes) const;

    void initEventMonitor();

    void reopenMonitorConnection();

    std::chrono::milliseconds reopenDelay() const;

    void addExpiredEvents(std::vector<DahuaEvent>& result);
private:
    void at_monitorResponseReceived();
    void at_monitorSomeBytesAvailable();

private:
    const Dahua::EngineManifest& m_engineManifest;
    nx::vms::api::analytics::DeviceAgentManifest m_deviceManifest;
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
        StartedEvent(const DahuaEvent& event = DahuaEvent()) :
            event(event)
        {
            timer.restart();
        }

        DahuaEvent event;
        nx::utils::ElapsedTimer timer;
    };

    QMap<QString, StartedEvent> m_startedEvents;
};

} // namespace dahua
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
