#pragma once

#include <string>
#include <unordered_set>
#include <exception>
#include <optional>

#include <nx/utils/url.h>
#include <nx/utils/move_only_func.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_async_client.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class ParameterApi: public nx::network::aio::BasicPollable
{
public:
    explicit ParameterApi(nx::utils::Url url);
    ~ParameterApi();

    void get(const std::unordered_set<std::string>& prefixes,
        nx::utils::MoveOnlyFunc<void(std::exception_ptr,
            std::unordered_map<std::string, std::string>)> handler);

    void set(const std::unordered_map<std::string, std::string>& parameters,
        nx::utils::MoveOnlyFunc<void(std::exception_ptr,
            std::unordered_map<std::string, std::string>)> handler);

    void cancel(nx::utils::MoveOnlyFunc<void()> handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    std::unordered_map<std::string, std::string> parseResponse();

private:
    nx::utils::Url m_url;
    std::optional<nx::network::http::AsyncClient> m_httpClient;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
