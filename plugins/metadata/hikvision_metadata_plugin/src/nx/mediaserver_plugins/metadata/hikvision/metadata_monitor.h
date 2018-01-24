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
#include <nx/utils/elapsed_timer.h>

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

class HikvisionMetadataMonitor: public QObject
{
    Q_OBJECT;
    using MetadataType = QString;
    using MetadataValue = QString;
    using MultipartContentParserPtr = std::unique_ptr<nx_http::MultipartContentParser>;

public:
    using Handler = std::function<void(const HikvisionEventList&)>;

    HikvisionMetadataMonitor(
        const Hikvision::DriverManifest& manifest,
        const QUrl& resourceUrl,
        const QAuthenticator& auth,
        const std::vector<QnUuid>& eventTypes);
    virtual ~HikvisionMetadataMonitor();

    void startMonitoring();
    void stopMonitoring();

    void addHandler(const QString& handlerId, const Handler& handler);
    void removeHandler(const QString& handlerId);
    void clearHandlers();

    bool processEvent(const const HikvisionEvent& hikvisionEvent);
private:
    QUrl buildMonitoringUrl(
        const QUrl& resourceUrl,
        const std::vector<QnUuid>& eventTypes) const;
    QUrl buildLprUrl(const QUrl& resourceUrl) const;

    void initMonitorUnsafe();
    void initEventMonitor();
    void initLprMonitor();

    void reopenMonitorConnection();
    void reopenLprConnection();
    void sendLprRequest();

    std::chrono::milliseconds reopenDelay() const;

    void addExpiredEvents(std::vector<HikvisionEvent>& result);
private:
    void at_responseReceived();
    void at_someBytesAvailable();
    void at_connectionClosed();

    void at_LprRequestDone();
private:
    const Hikvision::DriverManifest& m_manifest;
    const QUrl m_url;
    const QUrl m_lprUrl;
    const QAuthenticator m_auth;
    nx::network::aio::Timer m_timer;
    nx::network::aio::Timer m_lprTimer;
    QElapsedTimer m_timeSinceLastOpen;
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
    std::unique_ptr<nx_http::AsyncClient> m_lprHttpClient;
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
};

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx

