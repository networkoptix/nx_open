/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_FUSION_DATA_HTTP_CLIENT_H
#define NX_FUSION_DATA_HTTP_CLIENT_H

#include <functional>

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <common/common_globals.h>
#include <utils/common/stoppable.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/serialization/json.h>
#include <utils/serialization/lexical_functions.h>


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
    std::function<void(SystemError::ErrorCode, nx_http::StatusCode::Value, OutputData)> handler,
    SystemError::ErrorCode errCode,
    int statusCode,
    nx_http::BufferType msgBody)
{
    if (errCode != SystemError::noError ||
        statusCode != nx_http::StatusCode::ok)
    {
        handler(
            errCode,
            static_cast<nx_http::StatusCode::Value>(statusCode),
            OutputData());
        return;
    }

    bool success = false;
    OutputData outputData = QJson::deserialized<OutputData>(msgBody, OutputData(), &success);
    if (!success)
    {
        handler(SystemError::noError, nx_http::StatusCode::badRequest, OutputData());
        return;
    }

    handler(SystemError::noError, nx_http::StatusCode::ok, std::move(outputData));
}

template<typename HandlerFunc>
class BaseFusionDataHttpClient
:
    public QnStoppableAsync
{
public:
    BaseFusionDataHttpClient(QUrl url)
    :
        m_url(std::move(url)),
        m_httpClient(nx_http::AsyncHttpClient::create())
    {
        QObject::connect(
            m_httpClient.get(), &nx_http::AsyncHttpClient::done,
            m_httpClient.get(), [this](nx_http::AsyncHttpClientPtr client){ requestDone(client); },
            Qt::DirectConnection);
    }
    
    virtual ~BaseFusionDataHttpClient() {}

    //!Implementation of QnStoppableAsync::pleaseStopAsync
    virtual void pleaseStop( std::function<void()> handler ) override
    {
        m_httpClient->terminate();
        handler();
    }

    /*!
        \return \a false if failed to initiate async request
    */
    bool get(std::function<HandlerFunc> handler)
    {
        m_handler = std::move(handler);
        if (m_httpClient->doGet(m_url))
            return true;
        m_handler = std::function<HandlerFunc>();
        return false;
    }

protected:
    QUrl m_url;
    std::function<HandlerFunc> m_handler;

    virtual void requestDone(nx_http::AsyncHttpClientPtr client) = 0;

private:
    nx_http::AsyncHttpClientPtr m_httpClient;
};

}   //detail


//!HTTP client that uses \a fusion to serialize/deserialize input/output data
/*!
    If output data is expected, then only GET request can be used.
    Input data in this case is serialized to the url by calling \a serializeToUrlQuery(InputData, QUrlQuery*)
*/
template<typename InputData, typename OutputData>
class FusionDataHttpClient
:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, nx_http::StatusCode::Value, OutputData)>
{
    typedef detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, nx_http::StatusCode::Value, OutputData)> ParentType;

public:
    /*!
        TODO #ak if response Content-Type is multipart, then \a handler is invoked for every body part
    */
    FusionDataHttpClient(
        QUrl url,
        const InputData& input)
    :
        ParentType(std::move(url))
    {
        detail::serializeToUrl(input, &this->m_url);
    }
    
private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        detail::processHttpResponse(
            std::move(this->m_handler),
            client->response() ? SystemError::noError : SystemError::connectionReset,
            client->response() ? client->response()->statusLine.statusCode : 0,
            client->fetchMessageBodyBuffer());
    }
};

//!Specialization for void input data
template<typename OutputData>
class FusionDataHttpClient<void, OutputData>
:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, nx_http::StatusCode::Value, OutputData)>
{
    typedef detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, nx_http::StatusCode::Value, OutputData)> ParentType;

public:
    FusionDataHttpClient(QUrl url)
    :
        ParentType(std::move(url))
    {
    }
    
private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        detail::processHttpResponse(
            std::move(this->m_handler),
            client->response() ? SystemError::noError : SystemError::connectionReset,
            client->response() ? client->response()->statusLine.statusCode : 0,
            client->fetchMessageBodyBuffer());
    }
};

//!Specialization for void output data
template<typename InputData>
class FusionDataHttpClient<InputData, void>
:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, nx_http::StatusCode::Value)>
{
    typedef detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, nx_http::StatusCode::Value)> ParentType;

public:
    FusionDataHttpClient(
        QUrl url,
        const InputData& input)
    :
        ParentType(std::move(url))
    {
        detail::serializeToUrl(input, &m_url);
    }
    
private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        auto handler = std::move(this->m_handler);
        if (client->response())
            handler(
                SystemError::noError,
                static_cast<nx_http::StatusCode::Value>(
                    client->response()->statusLine.statusCode));
        else
            handler(
                SystemError::connectionReset,   //TODO #ak take error code from client
                nx_http::StatusCode::internalServerError);
    }
};

//!Specialization for both input & output data void
template<>
class FusionDataHttpClient<void, void>
:
    public detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, nx_http::StatusCode::Value)>
{
    typedef detail::BaseFusionDataHttpClient<
        void(SystemError::ErrorCode, nx_http::StatusCode::Value)> ParentType;

public:
    FusionDataHttpClient(QUrl url)
    :
        ParentType(std::move(url))
    {
    }
    
private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        auto handler = std::move(this->m_handler);
        if (client->response())
            handler(
                SystemError::noError,
                static_cast<nx_http::StatusCode::Value>(
                    client->response()->statusLine.statusCode));
        else
            handler(
                SystemError::connectionReset,   //TODO #ak take error code from client
                nx_http::StatusCode::internalServerError);
    }
};

}   //nx_http

#endif  //NX_FUSION_DATA_HTTP_CLIENT_H
