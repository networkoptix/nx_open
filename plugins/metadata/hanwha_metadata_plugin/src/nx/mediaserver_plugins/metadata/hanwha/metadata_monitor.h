#pragma once

#if defined(ENABLE_HANWHA)

#include <map>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>
#include <QtCore/QElapsedTimer>

#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/network/http/multipart_content_parser.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

class MetadataMonitor: public QObject
{
    Q_OBJECT;
    using MetadataType = QString;
    using MetadataValue = QString;
    using MultipartContentParserPtr = std::unique_ptr<nx::network::http::MultipartContentParser>;

public:
    using Handler = std::function<void(const EventList&)>;

    MetadataMonitor(
        const Hanwha::DriverManifest& manifest,
        const nx::utils::Url& resourceUrl,
        const QAuthenticator& auth);

    virtual ~MetadataMonitor();

    void startMonitoring();
    void stopMonitoring();

    void addHandler(const QString& handlerId, const Handler& handler);
    void removeHandler(const QString& handlerId);
    void clearHandlers();

    void setResourceAccess(const nx::utils::Url& url, const QAuthenticator& auth);

private:
    static nx::utils::Url buildMonitoringUrl(const nx::utils::Url& url);
    void initMonitorUnsafe();

private:
    void at_responseReceived(nx::network::http::AsyncHttpClientPtr httpClient);
    void at_someBytesAvailable(nx::network::http::AsyncHttpClientPtr httpClient);
    void at_connectionClosed(nx::network::http::AsyncHttpClientPtr httpClient);

private:
    const Hanwha::DriverManifest& m_manifest;
    nx::utils::Url m_url;
    QAuthenticator m_auth;
    nx::network::aio::Timer m_timer;
    QElapsedTimer m_timeSinceLastOpen;
    nx::network::http::AsyncHttpClientPtr m_httpClient;
    MultipartContentParserPtr m_contentParser;

    mutable QnMutex m_mutex;
    QMap<QString, Handler> m_handlers;
    std::atomic<bool> m_monitoringIsInProgress{false};
};

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

#endif // defined(ENABLE_HANWHA)
