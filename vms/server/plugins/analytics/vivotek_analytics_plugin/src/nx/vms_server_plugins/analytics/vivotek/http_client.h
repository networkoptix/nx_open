#pragma once

#include "nx/network/aio/basic_pollable.h"
#include <optional>

#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/url.h>
#include <nx/utils/move_only_func.h>
#include <nx/network/http/http_async_client.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class HttpClient: public nx::network::http::AsyncClient
{
public:
    HttpClient();

    cf::future<nx::network::http::BufferType> get(nx::utils::Url url);

    cf::future<nx::network::http::BufferType> post(nx::utils::Url url,
        nx::network::http::StringType contentType,
        nx::network::http::BufferType requestBody);

    using network::aio::BasicPollable::post;

private:
    nx::network::http::BufferType processResponse();
};

} // namespace nx::vms_server_plugins::analytics::vivotek
