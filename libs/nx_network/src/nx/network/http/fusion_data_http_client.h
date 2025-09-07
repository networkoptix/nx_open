// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/server/api_request_result.h>
#include <nx/reflect/json.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/serialization/format.h>
#include <nx/utils/url.h>

#include "detail/nxreflect_wrapper.h"

namespace nx::network::http {

namespace detail {

template<typename InputData>
void serializeToUrl(const InputData& data, QUrl* const url)
{
    QUrlQuery urlQuery;
    serializeToUrlQuery(data, &urlQuery);
    urlQuery.addQueryItem(
        "format", QString::fromStdString(nx::reflect::toString(Qn::SerializationFormat::json)));
    url->setQuery(urlQuery);
}

/**
 * Default overload for types that do not support this.
 */
template<typename T>
bool deserializeFromHeaders(
    const nx::network::http::HttpHeaders& /*from*/,
    T* /*what*/)
{
    return false;
}

template<typename HandlerFunc, typename SerializationLibWrapper>
class BaseFusionDataHttpClient:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;
    using self_type = BaseFusionDataHttpClient<HandlerFunc, SerializationLibWrapper>;

protected:
    using HttpClientPtr = std::unique_ptr<nx::network::http::AsyncClient,
        std::function<void(network::http::AsyncClient*)>>;

public:
    BaseFusionDataHttpClient(
        Url url, Credentials credentials, HttpClientPtr httpClient):
        m_url(std::move(url)),
        m_httpClient(std::move(httpClient))
    {
        NX_ASSERT(m_httpClient);

        m_httpClient->setCredentials(std::move(credentials));
        bindToAioThread(getAioThread());
    }

    BaseFusionDataHttpClient(
        Url url,
        Credentials credentials,
        ssl::AdapterFunc adapterFunc)
        :
        m_url(std::move(url)),
        m_httpClient(std::make_unique<nx::network::http::AsyncClient>(std::move(adapterFunc)))
    {
        m_httpClient->setCredentials(std::move(credentials));
        bindToAioThread(getAioThread());
    }

    BaseFusionDataHttpClient(
        nx::Url url,
        AuthInfo info,
        ssl::AdapterFunc adapterFunc,
        ssl::AdapterFunc proxyAdapterFunc)
        :
        m_url(std::move(url)),
        m_httpClient(std::make_unique<nx::network::http::AsyncClient>(std::move(adapterFunc)))
    {
        m_httpClient->setCredentials(std::move(info.credentials));
        m_httpClient->setProxyCredentials(std::move(info.proxyCredentials));
        m_httpClient->setProxyVia(
            std::move(info.proxyEndpoint), info.isProxySecure, std::move(proxyAdapterFunc));
        bindToAioThread(getAioThread());
    }

    virtual ~BaseFusionDataHttpClient() = default;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        base_type::bindToAioThread(aioThread);
        m_httpClient->bindToAioThread(aioThread);
    }

    void execute(nx::MoveOnlyFunc<HandlerFunc> handler)
    {
        m_handler = std::move(handler);
        if (!m_requestBody.empty())
            addRequestBody();
        auto completionHandler = std::bind(&self_type::requestDone, this, m_httpClient.get());

        if (m_requestContentType.empty())
            m_httpClient->doGet(m_url, std::move(completionHandler));
        else
            m_httpClient->doPost(m_url, std::move(completionHandler));
    }

    void execute(
        const Method& httpMethod,
        nx::MoveOnlyFunc<HandlerFunc> handler)
    {
        m_handler = std::move(handler);
        if (!m_requestBody.empty())
            addRequestBody();
        auto completionHandler = std::bind(&self_type::requestDone, this, m_httpClient.get());

        m_httpClient->doRequest(httpMethod, m_url, std::move(completionHandler));
    }

    void executeUpgrade(
        const std::string& protocolToUpgradeConnectionTo,
        nx::MoveOnlyFunc<HandlerFunc> handler)
    {
        executeUpgrade(
            nx::network::http::Method::options,
            protocolToUpgradeConnectionTo,
            std::move(handler));
    }

    void executeUpgrade(
        const Method& httpMethod,
        const std::string& protocolToUpgradeConnectionTo,
        nx::MoveOnlyFunc<HandlerFunc> handler)
    {
        m_handler = std::move(handler);
        if (!m_requestBody.empty())
            addRequestBody();

        m_httpClient->doUpgrade(
            m_url,
            httpMethod,
            protocolToUpgradeConnectionTo,
            std::bind(&self_type::requestDone, this, &m_httpClient));
    }

    void setRequestTimeout(std::chrono::milliseconds timeout)
    {
        m_httpClient->setSendTimeout(timeout);
        m_httpClient->setResponseReadTimeout(timeout);
        m_httpClient->setMessageBodyReadTimeout(timeout);
    }

    std::shared_ptr<AbstractStreamSocket> takeSocket()
    {
        return m_httpClient->takeSocket();
    }

    nx::network::http::AsyncClient& httpClient()
    {
        return *m_httpClient;
    }

    const nx::network::http::AsyncClient& httpClient() const
    {
        return *m_httpClient;
    }

    const nx::network::http::ApiRequestResult& lastFusionRequestResult() const
    {
        return m_lastFusionRequestResult;
    }

protected:
    nx::Url m_url;
    std::string m_requestContentType;
    nx::Buffer m_requestBody;
    nx::MoveOnlyFunc<HandlerFunc> m_handler;
    ApiRequestResult m_lastFusionRequestResult;

    template<typename T, typename = void>
    struct IsDeserializableWithNxReflect: std::false_type {};

    template<typename T>
    struct IsDeserializableWithNxReflect<
        T, std::enable_if_t<nx::reflect::IsInstrumentedV<T>>
    >: std::true_type {};

    template<typename T>
    struct IsDeserializableWithNxReflect<
        T, std::enable_if_t<nx::reflect::IsContainerV<T> &&
            nx::reflect::IsInstrumentedV<typename T::value_type>>
    >: std::true_type {};

    template<typename T>
    struct IsDeserializableWithNxReflect<
        T, std::enable_if_t<nx::reflect::IsAssociativeContainerV<T> &&
            nx::reflect::IsInstrumentedV<typename T::value_type::second_type> &&
            std::is_same_v<std::decay_t<typename T::value_type::first_type>, std::string>>
    >: std::true_type {};

    virtual void requestDone(nx::network::http::AsyncClient* client) = 0;

    void deserializeFusionRequestResult(
        SystemError::ErrorCode errCode,
        const nx::network::http::Response* response,
        const nx::Buffer& msgBody)
    {
        if (errCode != SystemError::noError ||
            !response ||
            !StatusCode::isSuccessCode(response->statusLine.statusCode))
        {
            nx::reflect::DeserializationResult result;
            std::tie(m_lastFusionRequestResult, result) =
                nx::reflect::json::deserialize<decltype(m_lastFusionRequestResult)>(msgBody);
            if (result)
                return;

            // Since it did not deserialize, it is not reliable, so clear its default values.
            m_lastFusionRequestResult.clear();

            // The message body has already been fetched to try and deserialize this structure.
            // If it failed, there may still be an error message in the body that should be saved.
            if (!msgBody.empty())
                m_lastFusionRequestResult.setErrorText(msgBody.toStdString());

            if (response)
                m_lastFusionRequestResult.setHttpStatusCode(response->statusLine.statusCode);
        }
    }

    template<typename OutputData>
    void processHttpResponse(
        nx::MoveOnlyFunc<void(
            SystemError::ErrorCode,
            const nx::network::http::Response*,
            OutputData)> handler,
        SystemError::ErrorCode errCode,
        const nx::network::http::Response* response,
        nx::Buffer msgBody)
    {
        using namespace nx::network::http;

        if (errCode != SystemError::noError ||
            !response ||
            !StatusCode::isSuccessCode(response->statusLine.statusCode))
        {
            deserializeFusionRequestResult(errCode, response, msgBody);
            handler(errCode, response, OutputData());
            return;
        }

        OutputData outputData;

        if (!msgBody.empty())
        {
            if constexpr (std::is_same_v<OutputData, QByteArray>)
            {
                outputData = msgBody.toByteArray();
            }
            else if (!SerializationLibWrapper::deserializeFromJson(msgBody, &outputData))
            {
                handler(SystemError::invalidData, response, OutputData());
                return;
            }
        }
        else
        {
            // Trying to read response data from HTTP headers.
            deserializeFromHeaders(response->headers, &outputData);
        }

        handler(SystemError::noError, response, std::move(outputData));
    }

private:
    HttpClientPtr m_httpClient;

    virtual void stopWhileInAioThread() override
    {
        m_httpClient->pleaseStopSync();
    }

    void addRequestBody()
    {
        decltype(m_requestBody) requestBody;
        requestBody.swap(m_requestBody);

        m_httpClient->setRequestBody(
            std::make_unique<nx::network::http::BufferSource>(
                m_requestContentType,
                std::move(requestBody)));
    }
};

} // namespace detail

/**
 * HTTP client that uses nx::reflect to serialize/deserialize input/output data.
 * If output data is expected, then only GET request can be used.
 * Input data in this case is serialized to the url by calling serializeToUrlQuery(InputData, QUrlQuery*).
 * NOTE: Reports SystemError::invalidData on failure to parse response.
 */
template<typename InputData, typename OutputData,
    typename SerializationLibWrapper = detail::NxReflectWrapper
>
class FusionDataHttpClient:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx::network::http::Response*, OutputData),
        SerializationLibWrapper>
{
    using base_type =
        detail::BaseFusionDataHttpClient<
            void(SystemError::ErrorCode, const nx::network::http::Response*, OutputData),
            SerializationLibWrapper>;

public:
    /**
     * TODO: #akolesnikov If response Content-Type is multipart, then handler is invoked for every body part.
     */
    FusionDataHttpClient(
        nx::Url url,
        Credentials credentials,
        ssl::AdapterFunc adapterFunc,
        const InputData& input)
        :
        base_type(std::move(url), std::move(credentials), std::move(adapterFunc))
    {
        init(input);
    }

    FusionDataHttpClient(
        Url url,
        Credentials credentials,
        base_type::HttpClientPtr httpClient,
        const InputData& input)
        :
        base_type(std::move(url), std::move(credentials), std::move(httpClient))
    {
        init(input);
    }

    FusionDataHttpClient(
        nx::Url url,
        AuthInfo info,
        ssl::AdapterFunc adapterFunc,
        ssl::AdapterFunc proxyAdapterFunc,
        const InputData& input)
        :
        base_type(
            std::move(url), std::move(info), std::move(adapterFunc), std::move(proxyAdapterFunc))
    {
        init(input);
    }

private:
    void init(const InputData& input)
    {
        bool ok = false;
        std::tie(this->m_requestBody, ok) = SerializationLibWrapper::serializeToJson(input);
        this->m_requestContentType = "application/json";
    }

    virtual void requestDone(nx::network::http::AsyncClient* client) override
    {
        decltype(this->m_handler) handler;
        handler.swap(this->m_handler);
        this->processHttpResponse(
            std::move(handler),
            client->failed() ? client->lastSysErrorCode() : SystemError::noError,
            client->response(),
            client->fetchMessageBodyBuffer());
    }
};

/**
 * Specialization for void input data.
 */
template<typename OutputData, typename SerializationLibWrapper>
class FusionDataHttpClient<void, OutputData, SerializationLibWrapper>:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx::network::http::Response*, OutputData),
        SerializationLibWrapper>
{
    using base_type = detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx::network::http::Response*, OutputData),
        SerializationLibWrapper>;

public:
    FusionDataHttpClient(
        nx::Url url, Credentials credentials, ssl::AdapterFunc adapterFunc):
        base_type(std::move(url), std::move(credentials), std::move(adapterFunc))
    {
    }

    FusionDataHttpClient(
        nx::Url url,
        AuthInfo info,
        ssl::AdapterFunc adapterFunc,
        ssl::AdapterFunc proxyAdapterFunc)
        :
        base_type(
            std::move(url), std::move(info), std::move(adapterFunc), std::move(proxyAdapterFunc))
    {
    }

    FusionDataHttpClient(
        Url url, Credentials credentials, base_type::HttpClientPtr httpClient):
        base_type(std::move(url), std::move(credentials), std::move(httpClient))
    {
    }

private:
    virtual void requestDone(nx::network::http::AsyncClient* client) override
    {
        decltype(this->m_handler) handler;
        handler.swap(this->m_handler);
        this->processHttpResponse(
            std::move(handler),
            client->failed() ? client->lastSysErrorCode() : SystemError::noError,
            client->response(),
            client->fetchMessageBodyBuffer());
    }
};

/**
 * Specialization for void output data.
 */
template<typename InputData, typename SerializationLibWrapper>
class FusionDataHttpClient<InputData, void, SerializationLibWrapper>:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx::network::http::Response*),
        SerializationLibWrapper>
{
    using base_type = detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx::network::http::Response*),
        SerializationLibWrapper>;

public:
    FusionDataHttpClient(
        nx::Url url,
        Credentials credentials,
        ssl::AdapterFunc adapterFunc,
        const InputData& input)
        :
        base_type(std::move(url), std::move(credentials), std::move(adapterFunc))
    {
        init(input);
    }

    FusionDataHttpClient(
        Url url,
        AuthInfo info,
        ssl::AdapterFunc adapterFunc,
        ssl::AdapterFunc proxyAdapterFunc,
        const InputData& input)
        :
        base_type(
            std::move(url), std::move(info), std::move(adapterFunc), std::move(proxyAdapterFunc))
    {
        init(input);
    }

    FusionDataHttpClient(
        Url url,
        Credentials credentials,
        base_type::HttpClientPtr httpClient,
        const InputData& input):
        base_type(std::move(url), std::move(credentials), std::move(httpClient))
    {
        init(input);
    }

private:
    void init(const InputData& input)
    {
        if constexpr (std::is_same_v<InputData, std::string>)
        {
            this->m_requestBody = input;
        }
        else
        {
            bool ok = false;
            std::tie(this->m_requestBody, ok) = SerializationLibWrapper::serializeToJson(input);
        }

        this->m_requestContentType = "application/json";
    }

private:
    virtual void requestDone(nx::network::http::AsyncClient* client) override
    {

        this->deserializeFusionRequestResult(
            client->lastSysErrorCode(),
            client->response(),
            client->fetchMessageBodyBuffer());

        decltype(this->m_handler) handler;
        handler.swap(this->m_handler);
        handler(
            client->lastSysErrorCode(),
            client->response());
    }
};

/**
 * Specialization for both input & output data void.
 */
template<typename SerializationLibWrapper>
class FusionDataHttpClient<void, void, SerializationLibWrapper>:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx::network::http::Response*),
        SerializationLibWrapper>
{
    using base_type = detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx::network::http::Response*),
        SerializationLibWrapper>;

public:
    FusionDataHttpClient(
        nx::Url url, Credentials credentials, ssl::AdapterFunc adapterFunc):
        base_type(std::move(url), std::move(credentials), std::move(adapterFunc))
    {
    }

    FusionDataHttpClient(
        nx::Url url,
        AuthInfo info,
        ssl::AdapterFunc adapterFunc,
        ssl::AdapterFunc proxyAdapterFunc)
        :
        base_type(
            std::move(url), std::move(info), std::move(adapterFunc), std::move(proxyAdapterFunc))
    {
    }

    FusionDataHttpClient(Url url, Credentials credentials, base_type::HttpClientPtr httpClient):
        base_type(std::move(url), std::move(credentials), std::move(httpClient))
    {
    }

private:
    virtual void requestDone(nx::network::http::AsyncClient* client) override
    {
        if (client->response())
        {
            client->moveMessageBodyToResponse();
            this->deserializeFusionRequestResult(
                client->lastSysErrorCode(),
                client->response(),
                client->response()->messageBody);
        }

        decltype(this->m_handler) handler;
        handler.swap(this->m_handler);
        handler(
            client->lastSysErrorCode(),
            client->response());
    }
};

} // namespace nx::network::http
