#if defined(ENABLE_HANWHA)

#include "metadata_monitor.h"

#include <chrono>

#include <nx/utils/std/cpp14.h>
#include "bytestream_filter.h"

#if defined(__GNUC__) || defined(__clang__)
    #define NX_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
    #define NX_PRETTY_FUNCTION __FUNCSIG__
#else
    #define NX_PRETTY_FUNCTION __func__
#endif

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace hanwha {

namespace {

static const QString kMonitorUrlTemplate =
    lit("http://%1:%2/stw-cgi/eventstatus.cgi?msubmenu=eventstatus&action=monitordiff");

static const int kDefaultHttpPort = 80;

static const std::chrono::minutes kKeepAliveTimeout{2};
static const std::chrono::seconds kUpdateInterval{10};

} // namespace

MetadataMonitor::MetadataMonitor(
    const Hanwha::DriverManifest& manifest,
    const nx::utils::Url& url,
    const QAuthenticator& auth)
    :
    m_manifest(manifest),
    m_url(buildMonitoringUrl(url)),
    m_auth(auth)
{
}

MetadataMonitor::~MetadataMonitor()
{
    stopMonitoring();
}

void MetadataMonitor::startMonitoring()
{
    m_timer.post([this](){ initMonitorUnsafe(); });
}

void MetadataMonitor::stopMonitoring()
{
    utils::promise<void> promise;
    m_timer.post(
        [this, &promise]()
        {
            if (m_httpClient)
                m_httpClient->pleaseStopSync();

            m_timer.pleaseStopSync();
            promise.set_value();
        });

    promise.get_future().wait();
    std::cout << "--------------" << NX_PRETTY_FUNCTION << std::endl;
}

void MetadataMonitor::addHandler(const QString& handlerId, const Handler& handler)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers[handlerId] = handler;
}

void MetadataMonitor::removeHandler(const QString& handlerId)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.remove(handlerId);
}

void MetadataMonitor::clearHandlers()
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.clear();
}

/*static*/ nx::utils::Url MetadataMonitor::buildMonitoringUrl(const nx::utils::Url &url)
{
    return nx::utils::Url(kMonitorUrlTemplate
        .arg(url.host())
        .arg(url.port(kDefaultHttpPort)));
}

void MetadataMonitor::initMonitorUnsafe()
{
    std::cout << "--------------" << NX_PRETTY_FUNCTION << std::endl;
    auto httpClient = nx_http::AsyncHttpClient::create();
    m_timer.pleaseStopSync();
    httpClient->bindToAioThread(m_timer.getAioThread());

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &MetadataMonitor::at_responseReceived,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
        this, &MetadataMonitor::at_someBytesAvailable,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &MetadataMonitor::at_connectionClosed,
        Qt::DirectConnection);

    httpClient->setTotalReconnectTries(nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setMessageBodyReadTimeoutMs(
        std::chrono::duration_cast<std::chrono::milliseconds>(kKeepAliveTimeout).count());

    auto handler =
        [this](const EventList& events)
        {
            QnMutexLocker lock(&m_mutex);
            for (const auto& handler: m_handlers)
                handler(events);
        };

    m_contentParser = std::make_unique<nx_http::MultipartContentParser>();
    m_contentParser->setForceParseAsBinary(true);
    m_contentParser->setNextFilter(std::make_shared<BytestreamFilter>(m_manifest, handler));

    httpClient->doGet(m_url);
    m_httpClient = httpClient;
}

void MetadataMonitor::at_responseReceived(nx_http::AsyncHttpClientPtr httpClient)
{
    const auto response = httpClient->response();
    if (response && response->statusLine.statusCode == nx_http::StatusCode::ok)
        m_contentParser->setContentType(httpClient->contentType());
    else
        at_connectionClosed(httpClient);
}

void MetadataMonitor::at_someBytesAvailable(nx_http::AsyncHttpClientPtr httpClient)
{
    const auto& buffer = httpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);
}

void MetadataMonitor::at_connectionClosed(nx_http::AsyncHttpClientPtr /*httpClient*/)
{
    m_timer.start(kUpdateInterval, [this]() { initMonitorUnsafe(); });
}

} // namespace hanwha
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

#endif // defined(ENABLE_HANWHA)
