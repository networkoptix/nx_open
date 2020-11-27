#pragma once

#include <optional>

#include <nx/utils/url.h>
#include <nx/utils/elapsed_timer.h>
#include <nx/network/aio/timer.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/multipart_content_parser.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_metadata_types.h>

#include "metadata_parser.h"

namespace nx::vms_server_plugins::analytics::dahua {

class DeviceAgent;

class MetadataMonitor
{
public:
    explicit MetadataMonitor(DeviceAgent* deviceAgent);

    void setNeededTypes(const nx::sdk::Ptr<const nx::sdk::analytics::IMetadataTypes>& types);

private:
    void start();
    void stop();
    void restart();
    nx::utils::Url buildUrl() const;

private:
    DeviceAgent* const m_deviceAgent;
    nx::sdk::Ptr<const nx::sdk::analytics::IMetadataTypes> m_neededTypes;
    nx::network::aio::Timer m_timer;
    nx::network::http::AsyncClient m_httpClient;
    std::optional<nx::network::http::MultipartContentParser> m_multipartContentParser;
    MetadataParser m_parser;
    nx::utils::ElapsedTimer m_sinceLastRestart;
};

} // namespace nx::vms_server_plugins::analytics::dahua
