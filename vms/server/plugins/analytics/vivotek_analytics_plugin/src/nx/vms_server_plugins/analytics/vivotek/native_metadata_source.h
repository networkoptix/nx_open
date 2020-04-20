#pragma once

#include <optional>
#include <exception>

#include <nx/kit/json.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/url.h>
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

    void open(const nx::utils::Url& baseUrl,
        nx::utils::MoveOnlyFunc<void(std::exception_ptr)> handler);

    void read(nx::utils::MoveOnlyFunc<void(std::exception_ptr, nx::kit::Json)> handler);

    void close(nx::utils::MoveOnlyFunc<void()> handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    void ensureDetailMetadataMode(nx::utils::Url url,
        nx::utils::MoveOnlyFunc<void(std::exception_ptr)> handler);

    void setDetailMetadataMode(nx::utils::Url url,
        nx::utils::MoveOnlyFunc<void(std::exception_ptr)> handler);

    void reloadConfig(nx::utils::Url url,
        nx::utils::MoveOnlyFunc<void(std::exception_ptr)> handler);

private:
    std::optional<nx::network::http::AsyncClient> m_httpClient;
    std::optional<nx::network::websocket::WebSocket> m_websocket;
    nx::Buffer m_buffer;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
