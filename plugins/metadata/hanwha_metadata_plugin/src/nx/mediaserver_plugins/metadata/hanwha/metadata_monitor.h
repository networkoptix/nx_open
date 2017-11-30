#pragma once

#include <map>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

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
    using MultipartContentParserPtr = std::unique_ptr<nx_http::MultipartContentParser>;

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

private:
    static nx::utils::Url buildMonitoringUrl(const nx::utils::Url& url);
    void initMonitorUnsafe();

private:
    void at_responseReceived(nx_http::AsyncHttpClientPtr httpClient);
    void at_someBytesAvailable(nx_http::AsyncHttpClientPtr httpClient);
    void at_connectionClosed(nx_http::AsyncHttpClientPtr httpClient);

private:
    const Hanwha::DriverManifest& m_manifest;
    mutable QnMutex m_mutex;
    const nx::utils::Url m_url;
    const QAuthenticator m_auth;
    nx_http::AsyncHttpClientPtr m_httpClient;
    MultipartContentParserPtr m_contentParser;
    QMap<QString, Handler> m_handlers;
    bool m_started = false;
};

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx
