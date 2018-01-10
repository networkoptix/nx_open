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

static const QString kMonitorUrlTemplate("/ISAPI/Event/notification/alertStream");

static const int kDefaultHttpPort = 80;
static const std::chrono::minutes kKeepAliveTimeout(2);
static const std::chrono::seconds kMinReopenInterval(10);

HikvisionMetadataMonitor::HikvisionMetadataMonitor(
    const Hikvision::DriverManifest& manifest,
    const nx::utils::Url& url,
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
    NX_VERBOSE(this, "Started");
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
    NX_VERBOSE(this, "Stopped");
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

nx::utils::Url HikvisionMetadataMonitor::buildMonitoringUrl(
    const nx::utils::Url& resourceUrl,
    const std::vector<QnUuid>& eventTypes) const
{
    int channel = std::max(1, QUrlQuery(resourceUrl.query()).queryItemValue("channel").toInt());
    QString eventListIds;
    for (const auto& eventTypeId: eventTypes)
    {
        auto name = m_manifest.eventDescriptorById(eventTypeId).internalName;
        eventListIds += lit("/%1-%2").arg(name).arg(channel).toLower();
    }

    nx::utils::Url monitorUrl(resourceUrl);
    monitorUrl.setPath(kMonitorUrlTemplate);
    return monitorUrl;
}

void HikvisionMetadataMonitor::initMonitorUnsafe()
{
    auto httpClient = std::make_unique<nx::network::http::AsyncClient>();
    m_timer.pleaseStopSync();
    httpClient->bindToAioThread(m_timer.getAioThread());

    httpClient->setOnResponseReceived([this]() { at_responseReceived(); });
    httpClient->setOnSomeMessageBodyAvailable([this]() { at_someBytesAvailable(); });
    httpClient->setOnDone([this]() { at_connectionClosed(); });

    m_timeSinceLastOpen.restart();
    httpClient->setTotalReconnectTries(nx::network::http::AsyncClient::UNLIMITED_RECONNECT_TRIES);
    httpClient->setUserName(m_auth.user());
    httpClient->setUserPassword(m_auth.password());
    httpClient->setMessageBodyReadTimeout(kKeepAliveTimeout);

    auto handler =
        [this](const HikvisionEventList& events)
        {
            QnMutexLocker lock(&m_mutex);
            for (const auto handler: m_handlers)
                handler(events);
        };

    m_contentParser = std::make_unique<nx::network::http::MultipartContentParser>();
    m_contentParser->setNextFilter(std::make_shared<BytestreamFilter>(m_manifest, handler));

    m_httpClient = std::move(httpClient);
    m_httpClient->doGet(m_url);
}

void HikvisionMetadataMonitor::at_responseReceived()
{
    if (!m_httpClient)
        return;
    const auto response = m_httpClient->response();
    if (response && response->statusLine.statusCode == nx::network::http::StatusCode::ok)
        m_contentParser->setContentType(m_httpClient->contentType());
    else
        at_connectionClosed();
}

void HikvisionMetadataMonitor::at_someBytesAvailable()
{
    if (!m_httpClient)
        return;
    const auto& buffer = m_httpClient->fetchMessageBodyBuffer();
    m_contentParser->processData(buffer);
}

void HikvisionMetadataMonitor::at_connectionClosed()
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
