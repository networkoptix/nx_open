#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/aio/async_operation_pool.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/url/url_builder.h>

#include "api_types.h"
#include "credentials.h"
#include "xml/deserialize.h"

namespace nx::cloud::aws {

class NX_AWS_CLIENT_API BaseApiClient:
    public nx::network::aio::BasicPollable
{
using base_type = nx::network::aio::BasicPollable;

public:
    BaseApiClient(
        const std::string& service,
        const std::string& awsRegion,
        const nx::utils::Url& url,
        const Credentials& credentials);
    ~BaseApiClient();

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    void setTimeouts(const nx::network::http::AsyncClient::Timeouts& timeouts);

protected:
    virtual void stopWhileInAioThread() override;

    std::unique_ptr<network::http::AsyncClient> prepareHttpClient();

    virtual void addAuthorizationToRequest(network::http::Request* request);


    virtual std::tuple<nx::String, bool> calculateAuthorizationHeader(
        const network::http::Request& request,
        const Credentials& credentials,
        const std::string& region,
        const std::string& service);

    template<typename Handler>
    void doAwsApiCall(
        const nx::network::http::Method::ValueType& method,
        const std::string& path,
        Handler handler,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> body = nullptr);

    template<typename Handler>
    void doAwsApiCall(
        const nx::network::http::Method::ValueType& method,
        const nx::utils::Url& url,
        Handler handler,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> body = nullptr);

    template<typename Output>
    void doAwsApiCall(
        const nx::network::http::Method::ValueType& method,
        const nx::utils::Url& url,
        nx::utils::MoveOnlyFunc<void(Result, Output)> handler,
        std::unique_ptr<nx::network::http::AbstractMsgBodySource> body = nullptr);

    ResultCode getResultCode(const nx::network::http::AsyncClient& httpClient) const;

protected:
    using RequestPool = nx::network::aio::AsyncOperationPool<nx::network::http::AsyncClient>;

    const std::string m_service;
    const std::string m_awsRegion;
    const nx::utils::Url m_url;
    const Credentials m_credentials;
    RequestPool m_requestPool;
    nx::network::http::AsyncClient::Timeouts m_timeouts;
};

template<typename Handler>
void BaseApiClient::doAwsApiCall(
    const nx::network::http::Method::ValueType& method,
    const std::string& path,
    Handler handler,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> body)
{
    doAwsApiCall(
        method,
        nx::network::url::Builder(m_url).appendPath(path),
        std::move(handler),
        std::move(body));
}

template<typename Handler>
void BaseApiClient::doAwsApiCall(
    const nx::network::http::Method::ValueType& method,
    const nx::utils::Url& url,
    Handler handler,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> body)
{
    using namespace nx::network;

    dispatch(
        [this, method, url, handler = std::move(handler), body = std::move(body)]() mutable
    {
        auto& context = m_requestPool.add(
            prepareHttpClient(),
            std::move(handler));

        if (body)
        {
            body->bindToAioThread(getAioThread());
            context.executor->setRequestBody(std::move(body));
        }

        context.executor->doRequest(
            method,
            url,
            [this, &context]() { m_requestPool.completeRequest(&context); });
    });
}

template<typename Output>
void BaseApiClient::doAwsApiCall(
    const nx::network::http::Method::ValueType& method,
    const nx::utils::Url& url,
    nx::utils::MoveOnlyFunc<void(Result, Output)> handler,
    std::unique_ptr<nx::network::http::AbstractMsgBodySource> body)
{
    doAwsApiCall(
        method,
        url,
        [this, handler = std::move(handler)](auto httpClient)
        {
            auto resultCode = getResultCode(*httpClient);
            if (resultCode != ResultCode::ok)
            {
                return handler(
                    Result(resultCode, httpClient->fetchMessageBodyBuffer().constData()),
                    Output());
            }

            Output output;
            auto messageBody = httpClient->fetchMessageBodyBuffer();
            if (!xml::deserialize(messageBody, &output))
            {
                auto error = lm("Failed to deserialize %1, string was %2")
                    .args(typeid(output).name(), messageBody.constData());
                NX_WARNING(this, error);
                return handler(Result(ResultCode::error, error.toStdString()), Output());
            }

            handler(Result(), std::move(output));
        },
        std::move(body));
}

} // namespace nx::cloud::aws