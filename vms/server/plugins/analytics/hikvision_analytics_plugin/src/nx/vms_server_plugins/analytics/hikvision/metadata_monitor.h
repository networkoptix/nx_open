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
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

class HikvisionMetadataMonitor: public QObject
{
    Q_OBJECT;
    using MetadataType = QString;
    using MetadataValue = QString;
    using MultipartContentParserPtr = std::unique_ptr<nx::network::http::MultipartContentParser>;

public:
    using Handler = std::function<void(const HikvisionEventList&)>;

    HikvisionMetadataMonitor(
        const Hikvision::EngineManifest& manifest,
        const nx::vms::api::analytics::DeviceAgentManifest& deviceManifest,
        const nx::utils::Url& resourceUrl,
        const QAuthenticator& auth,
        const std::vector<QString>& eventTypes);
    virtual ~HikvisionMetadataMonitor();

    void startMonitoring();
    void stopMonitoring();

    void addHandler(const QString& handlerId, const Handler& handler);
    void removeHandler(const QString& handlerId);
    void clearHandlers();

    bool processEvent(const HikvisionEvent& hikvisionEvent);

    void setDeviceInfo(const QString& deviceName, const QString& id);
private:
    nx::utils::Url buildMonitoringUrl(
        const nx::utils::Url& resourceUrl,
        const std::vector<QString>& eventTypes) const;
    nx::utils::Url buildLprUrl(const nx::utils::Url& resourceUrl) const;

    void initMonitorUnsafe();
    void initEventMonitor();
    void initLprMonitor();

    void reopenMonitorConnection();
    void reopenLprConnection();
    void sendLprRequest();

    std::chrono::milliseconds reopenDelay() const;

    void addExpiredEvents(std::vector<HikvisionEvent>& result);
private:
    void at_monitorResponseReceived();
    void at_monitorSomeBytesAvailable();

    void at_LprRequestDone();
private:
    const Hikvision::EngineManifest& m_manifest;
    nx::vms::api::analytics::DeviceAgentManifest m_deviceManifest;
    const nx::utils::Url m_monitorUrl;
    const nx::utils::Url m_lprUrl;
    const QAuthenticator m_auth;
    nx::network::aio::Timer m_monitorTimer;
    nx::network::aio::Timer m_lprTimer;
    QElapsedTimer m_timeSinceLastOpen;
    std::unique_ptr<nx::network::http::AsyncClient> m_monitorHttpClient;
    std::unique_ptr<nx::network::http::AsyncClient> m_lprHttpClient;
    MultipartContentParserPtr m_contentParser;
    QString m_fromDateFilter;

    mutable QnMutex m_mutex;
    QMap<QString, Handler> m_handlers;

    struct StartedEvent
    {
        StartedEvent(const HikvisionEvent& event = HikvisionEvent()) :
            event(event)
        {
            timer.restart();
        }

        HikvisionEvent event;
        nx::utils::ElapsedTimer timer;
    };

    QMap<QString, StartedEvent> m_startedEvents;

    QString m_deviceName;
    QString m_deviceId;

    /**
     * When a notification with unsupported event type arrives, we log it and store this
     * type in `m_receivedUnsupportedEventTypes` to provide exactly one record for each
     * event type.
     */
    std::set<QString> m_receivedUnsupportedEventTypes;
};

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
