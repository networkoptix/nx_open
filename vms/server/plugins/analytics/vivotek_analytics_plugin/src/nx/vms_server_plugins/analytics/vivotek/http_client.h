#pragma once

#include <optional>

#include <nx/utils/thread/cf/cfuture.h>
#include <nx/utils/url.h>
#include <nx/network/http/futures/client.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class HttpClient: public nx::network::http::futures::Client
{
public:
    HttpClient();

    cf::future<nx::network::http::BufferType> get(const nx::utils::Url& url);

    cf::future<nx::network::http::BufferType> post(const nx::utils::Url& url,
        nx::network::http::StringType contentType, nx::network::http::BufferType requestBody);

private:
    nx::network::http::BufferType processResponse(nx::network::http::Response&& response);
};

} // namespace nx::vms_server_plugins::analytics::vivotek
