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
#include <nx/network/http/asynchttpclient.h>
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
        handler(SystemError::noError, response, OutputData());
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
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_httpClient->terminate();
        handler();
    }

    void execute(std::function<HandlerFunc> handler)
    {
        m_handler = std::move(handler);
        if (m_requestContentType.isEmpty())
            m_httpClient->doGet(m_url);
        else
            m_httpClient->doPost(m_url, m_requestContentType, std::move(m_requestBody));
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
        const InputData& input)
    :
        ParentType(std::move(url))
    {
        this->m_requestBody = QJson::serialized(input);
        this->m_requestContentType =
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
        //detail::serializeToUrl(input, &this->m_url);
    }
    
private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        detail::processHttpResponse(
            std::move(this->m_handler),
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
        const InputData& input)
    :
        ParentType(std::move(url))
    {
        this->m_requestBody = QJson::serialized(input);
        this->m_requestContentType =
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
        //detail::serializeToUrl(input, &m_url);
    }
    
private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        auto handler = std::move(this->m_handler);
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
    FusionDataHttpClient(QUrl url)
    :
        ParentType(std::move(url))
    {
    }
    
private:
    virtual void requestDone(nx_http::AsyncHttpClientPtr client) override
    {
        auto handler = std::move(this->m_handler);
        handler(
            client->lastSysErrorCode(),
            client->response());
    }
};

}   //nx_http

#endif  //NX_FUSION_DATA_HTTP_CLIENT_H
