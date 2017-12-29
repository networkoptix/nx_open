#include "metadata_monitor.h"
#include "bytestream_filter.h"

#include <QtCore/QUrlQuery>

#include <chrono>
#include <iostream>

#include <nx/utils/std/cpp14.h>
#include <nx/utils/log/log_main.h>

namespace nx {
namespace mediaserver {
namespace plugins {
namespace hikvision {

static const QString kMonitorUrlTemplate("http://%1:%2/ISAPI/Event/notification/alertStream");

static const int kDefaultHttpPort = 80;
static const std::chrono::minutes kKeepAliveTimeout(2);
static const std::chrono::seconds kMinReopenInterval(10);

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
    NX_VERBOSE(this, "Monitor started");
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
    NX_VERBOSE(this, "Monitor stopped");
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

    m_timeSinceLastOpen.restart();
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
    m_contentParser->setNextFilter(std::make_shared<BytestreamFilter>(m_manifest, handler));

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
    const auto elapsed = m_timeSinceLastOpen.elapsed();
    std::chrono::milliseconds reopenDelay(std::max(0LL, (qint64) std::chrono::duration_cast
        <std::chrono::milliseconds>(kMinReopenInterval).count() - elapsed));
    m_timer.start(reopenDelay, [this]() { initMonitorUnsafe(); });
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver
} // namespace nx
