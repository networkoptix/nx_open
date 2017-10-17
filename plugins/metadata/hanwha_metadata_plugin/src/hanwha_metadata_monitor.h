#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <map>
#include <vector>

#include <hanwha_common.h>

#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/multipart_content_parser.h>

namespace nx {
namespace mediaserver {
namespace plugins {

class HanwhaMetadataMonitor: public QObject
{
    Q_OBJECT;
    using MetadataType = QString;
    using MetadataValue = QString;
    using MultipartContentParserPtr = std::unique_ptr<nx_http::MultipartContentParser>;

public:
    using Handler = std::function<void(const HanwhaEventList&)>;

    HanwhaMetadataMonitor(
        const Hanwha::DriverManifest& manifest,
        const nx::utils::Url& resourceUrl,
        const QAuthenticator& auth);
    virtual ~HanwhaMetadataMonitor();

    void startMonitoring();
    void stopMonitoring();

    void setHandler(const Handler& handler);

private:
    nx::utils::Url buildMonitoringUrl(const nx::utils::Url& url) const;
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
    Handler m_handler;
    bool m_started = false;
};

} // namespace plugins
} // namespace mediaserver
} // namespace nx
