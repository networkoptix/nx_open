#pragma once

#include <optional>

#include <nx/utils/url.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/network/buffer.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/websocket/websocket.h>

#include "http_client.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class WebSocket:
    public nx::network::aio::BasicPollable
{
public:
    WebSocket();
    ~WebSocket();

    cf::future<cf::unit> connect(nx::utils::Url url);
    cf::future<nx::Buffer> read();
    void close();

protected:
    virtual void stopWhileInAioThread() override;

private:
    HttpClient m_httpClient;
    std::optional<nx::network::websocket::WebSocket> m_nested;
    nx::Buffer m_buffer;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
