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

private:
    QUrl buildMonitoringUrl(
        const QUrl& resourceUrl,
        const std::vector<QnUuid>& eventTypes) const;
    void initMonitorUnsafe();

private:
    void at_responseReceived();
    void at_someBytesAvailable();
    void at_connectionClosed();

private:
    const Hikvision::DriverManifest& m_manifest;
    const QUrl m_url;
    const QAuthenticator m_auth;
    nx::network::aio::Timer m_timer;
    QElapsedTimer m_timeSinceLastOpen;
    std::unique_ptr<nx_http::AsyncClient> m_httpClient;
    MultipartContentParserPtr m_contentParser;

    mutable QnMutex m_mutex;
    QMap<QString, Handler> m_handlers;
};

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx

