#include "hanwha_metadata_monitor.h"
#include "hanwha_bytestream_filter.h"

#include <chrono>
#include <iostream>

#include <nx/utils/std/cpp14.h>

#if defined(__GNUC__) || defined(__clang__)
    #define NX_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
    #define NX_PRETTY_FUNCTION __FUNCSIG__
#else
    #define NX_PRETTY_FUNCTION __func__
#endif

namespace nx {
namespace mediaserver {
namespace plugins {

static const QString kMonitorUrlTemplate(
    "http://%1:%2/stw-cgi/eventstatus.cgi?msubmenu=eventstatus&action=monitordiff");

static const int kDefaultHttpPort = 80;
static const std::chrono::minutes kKeepAliveTimeout(2);
static const std::chrono::seconds kMinReopenInterval(10);

HanwhaMetadataMonitor::HanwhaMetadataMonitor(
    const Hanwha::DriverManifest& manifest,
    const QUrl& url,
    const QAuthenticator& auth)
    :
    m_manifest(manifest),
    m_url(buildMonitoringUrl(url)),
    m_auth(auth)
{
}

HanwhaMetadataMonitor::~HanwhaMetadataMonitor()
{
    stopMonitoring();
}

void HanwhaMetadataMonitor::startMonitoring()
{
    m_timer.post(
        [this]()
        {
            m_monitoringIsInProgress = true;
            initMonitorUnsafe();
        });
}

void HanwhaMetadataMonitor::stopMonitoring()
{
    utils::promise<void> promise;
    m_timer.post(
        [this, &promise]()
        {
            m_monitoringIsInProgress = false;
            if (m_httpClient)
                m_httpClient->pleaseStopSync();

            m_timer.pleaseStopSync();
            promise.set_value();
        });

    promise.get_future().wait();
    std::cout << "--------------" << NX_PRETTY_FUNCTION << std::endl;
}

void HanwhaMetadataMonitor::addHandler(const QString& handlerId, const Handler& handler)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers[handlerId] = handler;
}

void HanwhaMetadataMonitor::removeHandler(const QString& handlerId)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.remove(handlerId);
}

void HanwhaMetadataMonitor::clearHandlers()
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.clear();
}

void HanwhaMetadataMonitor::setResourceAccess(const QUrl& url, const QAuthenticator& auth)
{
    m_timer.post(
        [this, url, auth]()
        {
            m_url = buildMonitoringUrl(url);
            m_auth = auth;

            if (m_monitoringIsInProgress)
                initMonitorUnsafe();
        });
}

QUrl HanwhaMetadataMonitor::buildMonitoringUrl(const QUrl& url) const
{
    return QUrl(kMonitorUrlTemplate
        .arg(url.host())
        .arg(url.port(kDefaultHttpPort)));
}

void HanwhaMetadataMonitor::initMonitorUnsafe()
{
    if (!m_monitoringIsInProgress)
        return;

    std::cout << "--------------" << NX_PRETTY_FUNCTION << std::endl;
    auto httpClient = nx_http::AsyncHttpClient::create();
    m_timer.cancelSync();
    httpClient->bindToAioThread(m_timer.getAioThread());

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

    m_timeSinceLastOpen.restart();
    httpClient->setTotalReconnectTries(nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setMessageBodyReadTimeoutMs(
        std::chrono::duration_cast<std::chrono::milliseconds>(kKeepAliveTimeout).count());

    auto handler =
        [this](const HanwhaEventList& events)
        {
            QnMutexLocker lock(&m_mutex);
            for (const auto handler: m_handlers)
                handler(events);
        };

    m_contentParser = std::make_unique<nx_http::MultipartContentParser>();
    m_contentParser->setForceParseAsBinary(true);
    m_contentParser->setNextFilter(std::make_shared<HanwhaBytestreamFilter>(m_manifest, handler));

    httpClient->doGet(m_url);
    m_httpClient = httpClient;
}

void HanwhaMetadataMonitor::at_responseReceived(nx_http::AsyncHttpClientPtr httpClient)
{
    const auto response = httpClient->response();
    if (response && response->statusLine.statusCode == nx_http::StatusCode::ok)
        m_contentParser->setContentType(httpClient->contentType());
    else
        at_connectionClosed(httpClient);
}

void HanwhaMetadataMonitor::at_someBytesAvailable(nx_http::AsyncHttpClientPtr httpClient)
{
    const auto& buffer = httpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);
}

void HanwhaMetadataMonitor::at_connectionClosed(nx_http::AsyncHttpClientPtr httpClient)
{
    const auto elapsed = m_timeSinceLastOpen.elapsed();
    std::chrono::milliseconds reopenDelay(std::max(0LL, (qint64) std::chrono::duration_cast
        <std::chrono::milliseconds>(kMinReopenInterval).count() - elapsed));
    m_timer.start(reopenDelay, [this]() { initMonitorUnsafe(); });
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
