#pragma once 

#include <functional>

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <nx/fusion/serialization_format.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/fusion/serialization/lexical_functions.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/move_only_func.h>

namespace nx_http {
namespace detail {

template<typename InputData>
void serializeToUrl(const InputData& data, QUrl* const url)
{
    QUrlQuery urlQuery;
    serializeToUrlQuery(data, &urlQuery);
    urlQuery.addQueryItem("format", QnLexical::serialized(Qn::JsonFormat));
    url->setQuery(urlQuery);
}

template<typename OutputData>
void processHttpResponse(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, const nx_http::Response*, OutputData)> handler,
    SystemError::ErrorCode errCode,
    const nx_http::Response* response,
    nx_http::BufferType msgBody)
{
    if (errCode != SystemError::noError ||
        !response ||
        (response->statusLine.statusCode / 100) != (nx_http::StatusCode::ok / 100))
    {
        handler(
            errCode,
            response,
            OutputData());
        return;
    }

    bool success = false;
    OutputData outputData = QJson::deserialized<OutputData>(msgBody, OutputData(), &success);
    if (!success)
    {
        handler(SystemError::invalidData, response, OutputData());
        return;
    }

    handler(SystemError::noError, response, std::move(outputData));
}

template<typename HandlerFunc>
class BaseFusionDataHttpClient:
    public nx::network::aio::BasicPollable
{
    using base_type = nx::network::aio::BasicPollable;
    using self_type = BaseFusionDataHttpClient<HandlerFunc>;

public:
    BaseFusionDataHttpClient(QUrl url, AuthInfo auth):
        m_url(std::move(url))
    {
        m_httpClient.setAuth(auth);
        bindToAioThread(getAioThread());
    }

    virtual ~BaseFusionDataHttpClient() = default;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread)
    {
        base_type::bindToAioThread(aioThread);
        m_httpClient.bindToAioThread(aioThread);
    }

    void execute(nx::utils::MoveOnlyFunc<HandlerFunc> handler)
    {
        m_handler = std::move(handler);
        if (!m_requestBody.isEmpty())
            addRequestBody();
        auto completionHandler = std::bind(&self_type::requestDone, this, &m_httpClient);

        if (m_requestContentType.isEmpty())
            m_httpClient.doGet(m_url, std::move(completionHandler));
        else
            m_httpClient.doPost(m_url, std::move(completionHandler));
    }

    void execute(
        nx_http::Method::ValueType httpMethod,
        nx::utils::MoveOnlyFunc<HandlerFunc> handler)
    {
        m_handler = std::move(handler);
        if (!m_requestBody.isEmpty())
            addRequestBody();
        auto completionHandler = std::bind(&self_type::requestDone, this, &m_httpClient);

        m_httpClient.doRequest(httpMethod, m_url, std::move(completionHandler));
    }

    void executeUpgrade(
        const nx_http::StringType& protocolToUpgradeConnectionTo,
        nx::utils::MoveOnlyFunc<HandlerFunc> handler)
    {
        executeUpgrade(
            nx_http::Method::options,
            protocolToUpgradeConnectionTo,
            std::move(handler));
    }

    void executeUpgrade(
        nx_http::Method::ValueType httpMethod,
        const nx_http::StringType& protocolToUpgradeConnectionTo,
        nx::utils::MoveOnlyFunc<HandlerFunc> handler)
    {
        m_handler = std::move(handler);
        if (!m_requestBody.isEmpty())
            addRequestBody();

        m_httpClient.doUpgrade(
            m_url,
            httpMethod,
            protocolToUpgradeConnectionTo,
            std::bind(&self_type::requestDone, this, &m_httpClient));
    }

    void setRequestTimeout(std::chrono::milliseconds timeout)
    {
        m_httpClient.setSendTimeout(timeout);
        m_httpClient.setResponseReadTimeout(timeout);
        m_httpClient.setMessageBodyReadTimeout(timeout);
    }

    std::unique_ptr<AbstractStreamSocket> takeSocket()
    {
        return m_httpClient.takeSocket();
    }

    nx_http::AsyncClient& httpClient()
    {
        return m_httpClient;
    }

    const nx_http::AsyncClient& httpClient() const
    {
        return m_httpClient;
    }

protected:
    QUrl m_url;
    nx_http::StringType m_requestContentType;
    nx_http::BufferType m_requestBody;
    nx::utils::MoveOnlyFunc<HandlerFunc> m_handler;

    virtual void requestDone(nx_http::AsyncClient* client) = 0;

private:
    nx_http::AsyncClient m_httpClient;

    virtual void stopWhileInAioThread() override
    {
        m_httpClient.pleaseStopSync();
    }

    void addRequestBody()
    {
        decltype(m_requestBody) requestBody;
        requestBody.swap(m_requestBody);

        m_httpClient.setRequestBody(
            std::make_unique<nx_http::BufferSource>(
                m_requestContentType,
                std::move(requestBody)));
    }
};

} // namespace detail

/**
 * HTTP client that uses fusion to serialize/deserialize input/output data.
 * If output data is expected, then only GET request can be used.
 * Input data in this case is serialized to the url by calling serializeToUrlQuery(InputData, QUrlQuery*).
 * @note Reports SystemError::invalidData on failure to parse response.
 */
template<typename InputData, typename OutputData>
class FusionDataHttpClient:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*, OutputData)>
{
    using base_type =
        detail::BaseFusionDataHttpClient<
            void(SystemError::ErrorCode, const nx_http::Response*, OutputData)>;

public:
    /**
     * TODO: #ak If response Content-Type is multipart, then handler is invoked for every body part.
     */
    FusionDataHttpClient(
        QUrl url,
        AuthInfo auth,
        const InputData& input)
        :
        base_type(std::move(url), std::move(auth))
    {
        this->m_requestBody = QJson::serialized(input);
        this->m_requestContentType =
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
    }

private:
    virtual void requestDone(nx_http::AsyncClient* client) override
    {
        decltype(this->m_handler) handler;
        handler.swap(this->m_handler);
        detail::processHttpResponse(
            std::move(handler),
            client->failed() ? client->lastSysErrorCode() : SystemError::noError,
            client->response(),
            client->fetchMessageBodyBuffer());
    }
};

/**
 * Specialization for void input data.
 */
template<typename OutputData>
class FusionDataHttpClient<void, OutputData>:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*, OutputData)>
{
    typedef detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*, OutputData)> ParentType;

public:
    FusionDataHttpClient(QUrl url, AuthInfo auth):
        ParentType(std::move(url), std::move(auth))
    {
    }

private:
    virtual void requestDone(nx_http::AsyncClient* client) override
    {
        decltype(this->m_handler) handler;
        handler.swap(this->m_handler);
        detail::processHttpResponse(
            std::move(handler),
            client->failed() ? client->lastSysErrorCode() : SystemError::noError,
            client->response(),
            client->fetchMessageBodyBuffer());
    }
};

/** 
 * Specialization for void output data.
 */
template<typename InputData>
class FusionDataHttpClient<InputData, void>:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*)>
{
    typedef detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*)> ParentType;

public:
    FusionDataHttpClient(
        QUrl url,
        AuthInfo auth,
        const InputData& input)
        :
        ParentType(std::move(url), std::move(auth))
    {
        this->m_requestBody = QJson::serialized(input);
        this->m_requestContentType =
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
    }

private:
    virtual void requestDone(nx_http::AsyncClient* client) override
    {
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
template<>
class FusionDataHttpClient<void, void>:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*)>
{
    typedef detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*)> ParentType;

public:
    FusionDataHttpClient(QUrl url, AuthInfo auth):
        ParentType(std::move(url), std::move(auth))
    {
    }

private:
    virtual void requestDone(nx_http::AsyncClient* client) override
    {
        decltype(this->m_handler) handler;
        handler.swap(this->m_handler);
        handler(
            client->lastSysErrorCode(),
            client->response());
    }
};

} // namespace nx_http
