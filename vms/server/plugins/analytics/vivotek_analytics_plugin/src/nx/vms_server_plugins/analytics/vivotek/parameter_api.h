#pragma once

#include <string>
#include <unordered_set>
#include <optional>

#include <nx/utils/url.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/thread/cf/cfuture.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class ParameterApi: public nx::network::aio::BasicPollable
{
public:
    explicit ParameterApi(nx::utils::Url url);
    ~ParameterApi();

    cf::future<std::unordered_map<std::string, std::string>> get(
        std::unordered_set<std::string> prefixes);

    cf::future<std::unordered_map<std::string, std::string>>
        set(std::unordered_map<std::string, std::string> parameters);

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unordered_map<std::string, std::string> parseResponse();

private:
    nx::utils::Url m_url;
    std::optional<nx::network::http::AsyncClient> m_httpClient;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
