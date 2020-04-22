#pragma once

#include <optional>

#include <nx/kit/json.h>
#include <nx/utils/url.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/network/buffer.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/websocket/websocket.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class NativeMetadataSource:
    public nx::network::aio::BasicPollable
{
public:
    NativeMetadataSource();
    ~NativeMetadataSource();

    cf::future<cf::unit> open(const nx::utils::Url& url);
    cf::future<nx::kit::Json> read();
    void close();

protected:
    virtual void stopWhileInAioThread() override;

private:
    cf::future<cf::unit> ensureDetailMetadataMode(nx::utils::Url url);
    cf::future<cf::unit> setDetailMetadataMode(nx::utils::Url url);
    cf::future<cf::unit> reloadConfig(nx::utils::Url url);

private:
    std::optional<nx::network::http::AsyncClient> m_httpClient;
    std::optional<nx::network::websocket::WebSocket> m_websocket;
    nx::Buffer m_buffer;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
