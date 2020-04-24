#pragma once

#include <string>
#include <unordered_set>
#include <optional>

#include <nx/utils/url.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/thread/cf/cfuture.h>

#include "http_client.h"

namespace nx::vms_server_plugins::analytics::vivotek {

class ParameterApi: public nx::network::aio::BasicPollable
{
public:
    explicit ParameterApi(nx::utils::Url url);
    ~ParameterApi();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    cf::future<std::unordered_map<std::string, std::string>> get(
        std::unordered_set<std::string> prefixes);

    cf::future<std::unordered_map<std::string, std::string>>
        set(std::unordered_map<std::string, std::string> parameters);

    void cancel();

protected:
    virtual void stopWhileInAioThread() override;

private:
    nx::utils::Url m_url;
    HttpClient m_httpClient;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
