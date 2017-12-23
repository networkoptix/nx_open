#include "hikvision_metadata_monitor.h"
#include "hikvision_bytestream_filter.h"

#include <QtCore/QUrlQuery>

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

//static const QString kMonitorUrlTemplate("http://%1:%2/ISAPI/Event/triggers%3");
static const QString kMonitorUrlTemplate("http://%1:%2/ISAPI/Event/notification/alertStream");

static const int kDefaultHttpPort = 80;
static const std::chrono::minutes kKeepAliveTimeout(2);
static const std::chrono::seconds kUpdateInterval(10);

HikvisionMetadataMonitor::HikvisionMetadataMonitor(
    const Hikvision::DriverManifest& manifest,
    const QUrl& url,
    const QAuthenticator& auth,
    const std::vector<QnUuid>& eventTypes)
    :
    m_manifest(manifest),
    m_url(buildMonitoringUrl(url, eventTypes)),
    m_auth(auth)
{
}

HikvisionMetadataMonitor::~HikvisionMetadataMonitor()
{
    stopMonitoring();
}

void HikvisionMetadataMonitor::startMonitoring()
{
    m_timer.post([this](){ initMonitorUnsafe(); });
}

void HikvisionMetadataMonitor::stopMonitoring()
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

void HikvisionMetadataMonitor::addHandler(const QString& handlerId, const Handler& handler)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers[handlerId] = handler;
}

void HikvisionMetadataMonitor::removeHandler(const QString& handlerId)
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.remove(handlerId);
}

void HikvisionMetadataMonitor::clearHandlers()
{
    QnMutexLocker lock(&m_mutex);
    m_handlers.clear();
}

QUrl HikvisionMetadataMonitor::buildMonitoringUrl(
    const QUrl& url,
    const std::vector<QnUuid>& eventTypes) const
{
    int channel = std::max(1, QUrlQuery(url.query()).queryItemValue("channel").toInt());
    QString eventListIds;
    for (const auto& eventTypeId: eventTypes)
    {
        auto name = m_manifest.eventDescriptorById(eventTypeId).internalName;
        eventListIds += lit("/%1-%2").arg(name).arg(channel).toLower();
    }
    return QUrl(kMonitorUrlTemplate
        .arg(url.host())
        .arg(url.port(kDefaultHttpPort)));
        //.arg(eventListIds));
}

void HikvisionMetadataMonitor::initMonitorUnsafe()
{
    std::cout << "--------------" << NX_PRETTY_FUNCTION << std::endl;
    auto httpClient = nx_http::AsyncHttpClient::create();
    m_timer.pleaseStopSync();
    httpClient->bindToAioThread(m_timer.getAioThread());

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
        this, &HikvisionMetadataMonitor::at_responseReceived,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
        this, &HikvisionMetadataMonitor::at_someBytesAvailable,
        Qt::DirectConnection);

    connect(
        httpClient.get(), &nx_http::AsyncHttpClient::done,
        this, &HikvisionMetadataMonitor::at_connectionClosed,
        Qt::DirectConnection);

    httpClient->setTotalReconnectTries(nx_http::AsyncHttpClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setMessageBodyReadTimeoutMs(
        std::chrono::duration_cast<std::chrono::milliseconds>(kKeepAliveTimeout).count());

    auto handler =
        [this](const HikvisionEventList& events)
        {
            QnMutexLocker lock(&m_mutex);
            for (const auto handler: m_handlers)
                handler(events);
        };

    m_contentParser = std::make_unique<nx_http::MultipartContentParser>();
    m_contentParser->setForceParseAsBinary(true);
    m_contentParser->setNextFilter(std::make_shared<HikvisionBytestreamFilter>(m_manifest, handler));

    httpClient->doGet(m_url);
    m_httpClient = httpClient;
}

void HikvisionMetadataMonitor::at_responseReceived(nx_http::AsyncHttpClientPtr httpClient)
{
    const auto response = httpClient->response();
    if (response && response->statusLine.statusCode == nx_http::StatusCode::ok)
        m_contentParser->setContentType(httpClient->contentType());
    else
        at_connectionClosed(httpClient);
}

void HikvisionMetadataMonitor::at_someBytesAvailable(nx_http::AsyncHttpClientPtr httpClient)
{
    const auto& buffer = httpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);
}

void HikvisionMetadataMonitor::at_connectionClosed(nx_http::AsyncHttpClientPtr httpClient)
{
    m_timer.start(kUpdateInterval, [this]() { initMonitorUnsafe(); });
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx
