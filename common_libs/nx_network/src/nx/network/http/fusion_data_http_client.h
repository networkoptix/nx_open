#pragma once 

#include <functional>

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <nx/network/async_stoppable.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/fusion/serialization_format.h>
#include <nx/fusion/serialization/json.h>
#include <nx/fusion/serialization/lexical_functions.h>

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
    std::function<void(SystemError::ErrorCode, const nx_http::Response*, OutputData)> handler,
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
class BaseFusionDataHttpClient
:
    public QnStoppableAsync
{
public:
    BaseFusionDataHttpClient(QUrl url, AuthInfo auth)
    :
        m_url(std::move(url)),
        m_httpClient(nx_http::AsyncHttpClient::create())
    {
        QObject::connect(
            m_httpClient.get(), &nx_http::AsyncHttpClient::done,
            m_httpClient.get(), [this](nx_http::AsyncHttpClientPtr client){ requestDone(client); },
            Qt::DirectConnection);
        m_httpClient->setAuth(auth);
    }

    virtual ~BaseFusionDataHttpClient() {}

    //!Implementation of QnStoppableAsync::pleaseStopAsync
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_httpClient->pleaseStopSync();
        handler();
    }

    void execute(std::function<HandlerFunc> handler)
    {
        m_handler = std::move(handler);
        if (m_requestContentType.isEmpty())
        {
            m_httpClient->doGet(m_url);
        }
        else
        {
            decltype(m_requestBody) requestBody;
            requestBody.swap(m_requestBody);
            m_httpClient->doPost(m_url, m_requestContentType, std::move(requestBody));
        }
    }

    void setRequestTimeout(std::chrono::milliseconds timeout)
    {
        m_httpClient->setSendTimeoutMs(timeout.count());
        m_httpClient->setResponseReadTimeoutMs(timeout.count());
        m_httpClient->setMessageBodyReadTimeoutMs(timeout.count());
    }

protected:
    QUrl m_url;
    nx_http::StringType m_requestContentType;
    nx_http::BufferType m_requestBody;
    std::function<HandlerFunc> m_handler;

    virtual void requestDone(nx_http::AsyncHttpClientPtr client) = 0;

private:
    nx_http::AsyncHttpClientPtr m_httpClient;
};

} // namespace detail

//!HTTP client that uses \a fusion to serialize/deserialize input/output data
/*!
    If output data is expected, then only GET request can be used.
    Input data in this case is serialized to the url by calling \a serializeToUrlQuery(InputData, QUrlQuery*).
    \note Reports \a SystemError::invalidData on failure to parse response
*/
template<typename InputData, typename OutputData>
class FusionDataHttpClient
:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*, OutputData)>
{
    typedef detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*, OutputData)> ParentType;

public:
    /*!
        TODO #ak if response Content-Type is multipart, then \a handler is invoked for every body part
    */
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
        //detail::serializeToUrl(input, &this->m_url);
    }

private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
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

//!Specialization for void input data
template<typename OutputData>
class FusionDataHttpClient<void, OutputData>
:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*, OutputData)>
{
    typedef detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*, OutputData)> ParentType;

public:
    FusionDataHttpClient(QUrl url, AuthInfo auth)
    :
        ParentType(std::move(url), std::move(auth))
    {
    }

private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
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

//!Specialization for void output data
template<typename InputData>
class FusionDataHttpClient<InputData, void>
:
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
        //detail::serializeToUrl(input, &m_url);
    }

private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        decltype(this->m_handler) handler;
        handler.swap(this->m_handler);
        handler(
            client->lastSysErrorCode(),
            client->response());
    }
};

//!Specialization for both input & output data void
template<>
class FusionDataHttpClient<void, void>
:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*)>
{
    typedef detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, const nx_http::Response*)> ParentType;

public:
    FusionDataHttpClient(QUrl url, AuthInfo auth)
    :
        ParentType(std::move(url), std::move(auth))
    {
    }

private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        decltype(this->m_handler) handler;
        handler.swap(this->m_handler);
        handler(
            client->lastSysErrorCode(),
            client->response());
    }
};

} // namespace nx_http
