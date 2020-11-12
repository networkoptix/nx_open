#pragma once

#include <optional>

#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/url.h>
#include <nx/network/buffer.h>
#include <nx/network/websocket/websocket.h>

#include "http_client.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class WebSocket
{
public:
    cf::future<cf::unit> open(const nx::utils::Url& url);
    cf::future<nx::Buffer> read();
    void close();

private:
    nx::Buffer m_buffer;
    std::optional<HttpClient> m_httpClient;
    std::optional<nx::network::websocket::WebSocket> m_nested;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
