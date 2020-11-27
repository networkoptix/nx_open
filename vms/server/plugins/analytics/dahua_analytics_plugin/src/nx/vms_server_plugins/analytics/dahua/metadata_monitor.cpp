#include "metadata_monitor.h"

#include <chrono>
#include <memory>
#include <vector>

#include <nx/utils/log/log_main.h>
#include <nx/utils/string.h>

#include "device_agent.h"

namespace nx::vms_server_plugins::analytics::dahua {

using namespace std::literals;
using namespace nx::utils;
using namespace nx::network;
using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

constexpr auto kKeepAliveTimeout = 2min;
constexpr auto kMinRestartInterval = 10s;

} // namespace

MetadataMonitor::MetadataMonitor(DeviceAgent* deviceAgent):
    m_deviceAgent(deviceAgent),
    m_parser(deviceAgent)
{
    m_httpClient.bindToAioThread(m_timer.getAioThread());

    const auto& deviceInfo = m_deviceAgent->info();
    m_httpClient.setUserName(deviceInfo->login());
    m_httpClient.setUserPassword(deviceInfo->password());

    m_httpClient.setTotalReconnectTries(http::AsyncClient::UNLIMITED_RECONNECT_TRIES);
    m_httpClient.setMessageBodyReadTimeout(kKeepAliveTimeout);

    m_httpClient.setOnResponseReceived(
        [this]()
        {
            if (!m_httpClient.hasRequestSucceeded())
            {
                if (const auto response = m_httpClient.response(); NX_ASSERT(response))
                {
                    http::BufferType bytes;
                    response->serialize(&bytes);
                    NX_DEBUG(this, "Failed to start:\n%1", bytes);
                }

                restart();
                return;
            }

            m_multipartContentParser.emplace();
            m_multipartContentParser->setContentType(m_httpClient.contentType());
            m_multipartContentParser->setNextFilter(
                std::shared_ptr<MetadataParser>(&m_parser, [](auto) {}));
        });

    m_httpClient.setOnSomeMessageBodyAvailable(
        [this]()
        {
            m_multipartContentParser->processData(m_httpClient.fetchMessageBodyBuffer());
        });

    m_httpClient.setOnDone(
        [this]()
        {
            if (m_httpClient.failed())
            {
                NX_DEBUG(this, "HTTP connection failed: %1",
                    SystemError::toString(m_httpClient.lastSysErrorCode()));
            }

            restart();
        });
}

void MetadataMonitor::setNeededTypes(const Ptr<const IMetadataTypes>& types)
{
    m_timer.executeInAioThreadSync(
        [&]()
        {
            stop();

            m_neededTypes = types;

            start();
        });
}

void MetadataMonitor::start()
{
    if (!m_neededTypes || m_neededTypes->isEmpty())
        return;

    const auto url = buildUrl();

    NX_DEBUG(this, "Starting %1", url);

    m_httpClient.doGet(url);
}

void MetadataMonitor::stop()
{
    m_timer.pleaseStopSync();
    m_httpClient.pleaseStopSync();
    m_parser.terminateOngoingEvents();

    NX_DEBUG(this, "Stopped");
}

void MetadataMonitor::restart()
{
    stop();

    m_timer.start(std::max(0ms, kMinRestartInterval - m_sinceLastRestart.elapsed()),
        [this]()
        {
            m_sinceLastRestart.restart();

            start();
        });
}

Url MetadataMonitor::buildUrl() const
{
    const auto& deviceInfo = m_deviceAgent->info();

    Url url = deviceInfo->url();
    url.setPath("/cgi-bin/eventManager.cgi");

    std::vector<QString> allNativeIds;

    const auto eventTypeIds = m_neededTypes->eventTypeIds();
    for (int i = 0; i < eventTypeIds->count(); ++i)
    {
        const QString id = eventTypeIds->at(i);

        const auto type = EventType::findById(id);
        if (!NX_ASSERT(type))
            continue;

        const auto& nativeIds = type->nativeIds;
        allNativeIds.insert(allNativeIds.begin(), nativeIds.begin(), nativeIds.end());
    }

    url.setQuery(NX_FMT("action=attach&codes=[%1]&heartbeat=3",
        nx::utils::join(allNativeIds, ",")));

    return url;
}

} // namespace nx::vms_server_plugins::analytics::dahua
