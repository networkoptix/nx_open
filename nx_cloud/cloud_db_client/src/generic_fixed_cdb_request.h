/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_GENERIC_FIXED_CDB_REQUEST_H
#define NX_CDB_CL_GENERIC_FIXED_CDB_REQUEST_H

#include <functional>

#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <common/common_globals.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/serialization/json.h>
#include <utils/serialization/lexical_functions.h>

#include "data/types.h"


namespace nx {
namespace cdb {
namespace cl {

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
    std::function<void(api::ResultCode, OutputData)> handler,
    SystemError::ErrorCode errCode,
    int statusCode,
    nx_http::BufferType msgBody)
{
    if (errCode != SystemError::noError)
    {
        handler(api::ResultCode::networkError, OutputData());
        return;
    }

    if (statusCode != nx_http::StatusCode::ok)
    {
        handler(api::httpStatusCodeToResultCode(
            static_cast<nx_http::StatusCode::Value>(statusCode)),
            OutputData());
        return;
    }

    bool success = false;
    OutputData outputData = QJson::deserialized<OutputData>(msgBody, OutputData(), &success);
    if (!success)
    {
        handler(api::ResultCode::networkError, OutputData());
        return;
    }

    handler(api::ResultCode::ok, std::move(outputData));
}

//!Overload for void output data
void processHttpResponseNoOutput(
    std::function<void(api::ResultCode)> handler,
    SystemError::ErrorCode errCode,
    int statusCode,
    nx_http::BufferType /*msgBody*/)
{
    if (errCode != SystemError::noError)
    {
        handler(api::ResultCode::networkError);
        return;
    }

    if (statusCode != nx_http::StatusCode::ok)
    {
        handler(api::httpStatusCodeToResultCode(
            static_cast<nx_http::StatusCode::Value>(statusCode)));
        return;
    }

    handler(api::ResultCode::ok);
}

//!Sends request serializing as http GET request with \a input serialized to query and expecting output as json message body
/*!
    \warning On failure to initiate async request, \a handler is invoked directly from this call
*/
template<class InputData, class OutputData>
void doRequest(
    QUrl url,
    InputData input,
    std::function<void(api::ResultCode, OutputData)> handler )
{
    serializeToUrl(input, &url);
    
    using namespace std::placeholders;
    if (!nx_http::downloadFileAsync(
            std::move(url),
            std::bind(&processHttpResponse<OutputData>, std::move(handler), _1, _2, _3)))
    {
        handler(api::ResultCode::networkError, OutputData());
    }
}

//!Overload for void input data
template<class OutputData>
void doRequest(
    QUrl url,
    std::function<void(api::ResultCode, OutputData)> handler)
{
    using namespace std::placeholders;
    if (!nx_http::downloadFileAsync(
        std::move(url),
        std::bind(&processHttpResponse<OutputData>, std::move(handler), _1, _2, _3)))
    {
        handler(api::ResultCode::networkError, OutputData());
    }
}

//!Overload for void output data
template<class InputData>
void doRequest(
    QUrl url,
    InputData input,
    std::function<void(api::ResultCode)> handler)
{
    serializeToUrl(input, &url);

    using namespace std::placeholders;
    if (!nx_http::downloadFileAsync(
        std::move(url),
        std::bind(&processHttpResponseNoOutput, std::move(handler), _1, _2, _3)))
    {
        handler(api::ResultCode::networkError);
    }
}

//!Overload for void input & output
void doRequest(
    QUrl url,
    std::function<void(api::ResultCode)> handler)
{
    using namespace std::placeholders;
    if (!nx_http::downloadFileAsync(
        std::move(url),
        std::bind(&processHttpResponseNoOutput, std::move(handler), _1, _2, _3)))
    {
        handler(api::ResultCode::networkError);
    }
}


namespace detail {

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
    bool get()
    {
        return m_httpClient->doGet(m_url);
    }
    
protected:
    QUrl m_url;

    virtual void requestDone(nx_http::AsyncHttpClientPtr client) = 0;

private:
    nx_http::AsyncHttpClientPtr m_httpClient;
};

}


//!HTTP client that uses \a fusion to serialize/deserialize
template<typename InputData, typename OutputData>
class FusionDataHttpClient
:
    public detail::BaseFusionDataHttpClient
{
public:
    /*!
        TODO #ak if response Content-Type is multipart, then \a handler is invoked for every body part
    */
    FusionDataHttpClient(
        QUrl url,
        const InputData& input,
        std::function<void(api::ResultCode, OutputData)> handler)
    :
        detail::BaseFusionDataHttpClient(std::move(url)),
        m_handler(std::move(handler))
    {
        serializeToUrl(input, &m_url);
    }
    
private:
    std::function<void(api::ResultCode, OutputData)> m_handler;
    
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        processHttpResponse(
            std::move(m_handler),
            client->response() ? SystemError::noError : SystemError::connectionReset,
            client->response() ? client->response()->statusList.statusCode : 0,
            client->fetchMessageBodyBuffer());
    }
};

template<typename OutputData>
class FusionDataHttpClient<void, OutputData>
:
    public detail::BaseFusionDataHttpClient
{
public:
    FusionDataHttpClient(
        QUrl url,
        std::function<void(api::ResultCode, OutputData)> handler)
    :
        detail::BaseFusionDataHttpClient(std::move(url)),
        m_handler(std::move(handler))
    {
    }
    
private:
    std::function<void(api::ResultCode, OutputData)> m_handler;
    
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        processHttpResponse(
            std::move(m_handler),
            client->response() ? SystemError::noError : SystemError::connectionReset,
            client->response() ? client->response()->statusList.statusCode : 0,
            client->fetchMessageBodyBuffer());
    }
};

template<typename InputData>
class FusionDataHttpClient<InputData, void>
:
    public detail::BaseFusionDataHttpClient
{
public:
    FusionDataHttpClient(
        QUrl url,
        const InputData& input,
        std::function<void(api::ResultCode)> handler)
    :
        detail::BaseFusionDataHttpClient(std::move(url)),
        m_handler(std::move(handler))
    {
        serializeToUrl(input, &m_url);
    }
    
private:
    std::function<void(api::ResultCode)> m_handler;
    
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        m_handler(
            client->response()
            ? api::httpStatusCodeToResultCode(
                static_cast<nx_http::StatusCode::Value>(client->response()->statusList.statusCode))
            : api::ResultCode::networkError);
    }
};

template<>
class FusionDataHttpClient<void, void>
:
    public detail::BaseFusionDataHttpClient
{
public:
    FusionDataHttpClient(
        QUrl url,
        std::function<void(api::ResultCode)> handler)
    :
        detail::BaseFusionDataHttpClient(std::move(url)),
        m_handler(std::move(handler))
    {
    }
    
private:
    std::function<void(api::ResultCode)> m_handler;
    
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        m_handler(
            client->response()
            ? api::httpStatusCodeToResultCode(
                static_cast<nx_http::StatusCode::Value>(client->response()->statusList.statusCode))
            : api::ResultCode::networkError);
    }
};

}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_GENERIC_FIXED_CDB_REQUEST_H
