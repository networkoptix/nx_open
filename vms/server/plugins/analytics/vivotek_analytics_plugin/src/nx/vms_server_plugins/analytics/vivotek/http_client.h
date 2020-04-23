#pragma once

#include <optional>

#include <nx/utils/url.h>
#include <nx/utils/thread/cf/cfuture.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_async_client.h>

namespace nx::vms_server_plugins::analytics::vivotek {

class HttpClient:
    public nx::network::aio::BasicPollable
{
public:
    HttpClient();
    ~HttpClient();

    const nx::network::http::Request& request();

    void setAdditionalRequestHeaders(nx::network::http::HttpHeaders headers);
    void addAdditionalRequestHeader(
        const nx::network::http::StringType& name, const nx::network::http::StringType& value);
    void addAdditionalRequestHeaders(const nx::network::http::HttpHeaders& headers);
    void removeAdditionalRequestHeader(const nx::network::http::StringType& name);

    void setRequestBody(std::unique_ptr<nx::network::http::AbstractMsgBodySource> body);

    cf::future<nx::network::http::Response> request(
        nx::network::http::Method::ValueType method, nx::utils::Url url);
    cf::future<nx::network::http::Response> get(nx::utils::Url url);
    cf::future<nx::network::http::Response> post(nx::utils::Url url);
    using BasicPollable::post;

    void cancel();

    std::unique_ptr<nx::network::AbstractStreamSocket> takeSocket();

protected:
    virtual void stopWhileInAioThread() override;

    nx::network::http::AsyncClient& nested();

private:
    std::optional<nx::network::http::AsyncClient> m_nested;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
