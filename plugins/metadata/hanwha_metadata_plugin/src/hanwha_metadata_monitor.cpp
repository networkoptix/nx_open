#include "hanwha_metadata_monitor.h"
#include "hanwha_bytestream_filter.h"

#include <chrono>
#include <iostream>

#include <nx/utils/std/cpp14.h>

namespace nx {
namespace mediaserver {
namespace plugins {

namespace {
    const QString kMonitorUrlTemplate(
        "http://%1:%2/stw-cgi/eventstatus.cgi?msubmenu=eventstatus&action=monitor");

    const int kDefaultHttpPort = 80;

    const std::chrono::minutes kKeepAliveTimeout(7);
} // namespace 


HanwhaMetadataMonitor::HanwhaMetadataMonitor(const QUrl& url, const QAuthenticator& auth):
    m_url(buildMonitoringUrl(url)),
    m_auth(auth)
{

}

HanwhaMetadataMonitor::~HanwhaMetadataMonitor()
{

}

void HanwhaMetadataMonitor::startMonitoring()
{
    auto httpClient = nx_http::AsyncHttpClient::create();

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &HanwhaMetadataMonitor::at_responseReceived,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
        this, &HanwhaMetadataMonitor::at_someBytesAvailable,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &HanwhaMetadataMonitor::at_connectionClosed,
        Qt::DirectConnection);

    httpClient->setTotalReconnectTries(nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setMessageBodyReadTimeoutMs(
        std::chrono::duration_cast<std::chrono::milliseconds>(kKeepAliveTimeout).count());

    auto handler = [this](const HanwhaEventList& events) { m_handler(events); };

    m_contentParser = std::make_unique<nx_http::MultipartContentParser>();
    m_contentParser->setForceParseAsBinary(true);
    m_contentParser->setNextFilter(std::make_shared<HanwhaBytestreamFilter>(handler));

    httpClient->doGet(m_url);

    m_httpClient = httpClient;
}

void HanwhaMetadataMonitor::stopMonitoring()
{
    m_httpClient->pleaseStopSync();
}

void HanwhaMetadataMonitor::setHandler(const Handler& handler)
{
    m_handler = handler;
}

QUrl HanwhaMetadataMonitor::buildMonitoringUrl(const QUrl& url) const
{
    return QUrl(kMonitorUrlTemplate
        .arg(url.host())
        .arg(url.port(kDefaultHttpPort)));
}

void HanwhaMetadataMonitor::at_responseReceived(nx_http::AsyncHttpClientPtr httpClient)
{
    m_contentParser->setContentType(httpClient->contentType());
}

void HanwhaMetadataMonitor::at_someBytesAvailable(nx_http::AsyncHttpClientPtr httpClient)
{
    const auto& buffer = httpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);
}

void HanwhaMetadataMonitor::at_connectionClosed(nx_http::AsyncHttpClientPtr httpClient)
{
    std::cout << "Hanwha monitor connection closed" << std::endl;
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
