#pragma once

#include <optional>

#include <nx/kit/json.h>
#include <nx/utils/url.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/network/aio/basic_pollable.h>

#include "http_client.h"
#include "websocket.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class NativeMetadataSource:
    public nx::network::aio::BasicPollable
{
public:
    NativeMetadataSource();
    ~NativeMetadataSource();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    cf::future<cf::unit> open(const nx::utils::Url& url);
    cf::future<nx::kit::Json> read();
    void close();

protected:
    virtual void stopWhileInAioThread() override;

private:
    cf::future<bool> getDetailMetadataMode(nx::utils::Url url);
    cf::future<cf::unit> setDetailMetadataMode(const nx::utils::Url& url, bool value);
    cf::future<cf::unit> reloadConfig(nx::utils::Url url);

private:
    HttpClient m_httpClient;
    WebSocket m_websocket;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
