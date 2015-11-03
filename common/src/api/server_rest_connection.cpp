#include "server_rest_connection.h"

#include <api/app_server_connection.h>
#include <api/helpers/chunks_request_data.h>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx_ec/data/api_data.h>

#include <utils/common/model_functions.h>
#include <utils/network/router.h>
#include <http/custom_headers.h>
#include "utils/network/http/httptypes.h"
#include <utils/network/http/asynchttpclient.h>

namespace {
    static const size_t ResponseReadTimeoutMs = 15 * 1000;
    static const size_t TcpConnectTimeoutMs   = 5 * 1000;
}

// --------------------------- public methods -------------------------------------------

QnServerRestConnection::QnServerRestConnection(const QnUuid& serverId):
    m_serverId(serverId)
{

}

QnServerRestConnection::~QnServerRestConnection()
{
    for (auto value: m_runningRequests)
        value->terminate();
}

QnUuid QnServerRestConnection::cameraHistoryAsync(const QnChunksRequestData &request, Result<ec2::ApiCameraHistoryDataList>::type callback)
{
    return executeGet(lit("/ec2/cameraHistory"), request.toParams(), callback);
}

// --------------------------- private implementation -------------------------------------

QUrl QnServerRestConnection::prepareUrl(const QString& path, const QnRequestParamList& params) const
{
    QUrl result;
    result.setPath(path);
    QUrlQuery q;
    for (const auto& param: params)
        q.addQueryItem(param.first, param.second);
    result.setQuery(q.toString());
    return result;
}

template <class T>
T parseMessageBody(const Qn::SerializationFormat& format, const nx_http::BufferType& msgBody, bool* success)
{
    switch(format) {
    case Qn::JsonFormat:
        return QJson::deserialized(msgBody, T(), success);
    case Qn::UbjsonFormat:
        return QnUbjson::deserialized(msgBody, T(), success);
    default:
        break;
    }
    return T();
}

template <typename ResultType>
QnUuid QnServerRestConnection::executeGet(const QString& path, const QnRequestParamList& params, std::function<void (bool, ResultType)> callback)
{
    Request request = prepareRequest(HttpMethod::Get, prepareUrl(path, params));
    if (!request.isValid())
        return QnUuid();
    return executeRequest(request, callback);
}

template <typename ResultType>
QnUuid QnServerRestConnection::executePost(const QString& path, 
                                           const QnRequestParamList& params,
                                           const nx_http::StringType& contentType, 
                                           const nx_http::StringType& messageBody, 
                                           std::function<void (bool, ResultType)> callback)
{
    Request request = prepareRequest(HttpMethod::Post, prepareUrl(path, params), contentType, messageBody);
    if (!request.isValid())
        return QnUuid();
    return executeRequest(request, callback);
}

template <typename ResultType>
QnUuid QnServerRestConnection::executeRequest(const Request& request, std::function<void (bool, ResultType)> callback)
{
    return sendRequest(request, [callback] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::StringType contentType, nx_http::BufferType msgBody)
    {
        if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok)
        {
            bool success = false;
            Qn::SerializationFormat format = Qn::serializationFormatFromHttpContentType(contentType);
            ResultType result = parseMessageBody<ResultType>(format, msgBody, &success);
            callback(success, result);
        }
        else
            callback(false, ResultType());
    });
}

QnUuid QnServerRestConnection::executeRequest(const Request& request, std::function<void (bool, DummyContentType)> callback)
{
    return sendRequest(request, [callback] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::StringType, nx_http::BufferType)
    {
        bool ok = osErrorCode == SystemError::noError && statusCode >= nx_http::StatusCode::ok && statusCode <= nx_http::StatusCode::partialContent;
        callback(ok, DummyContentType());
    });
}

void QnServerRestConnection::cancelRequest(const QnUuid& requestId)
{
    nx_http::AsyncHttpClientPtr httpClient;
    {
        QnMutexLocker lock(&m_mutex);
        auto itr = m_runningRequests.find(requestId);
        if (itr != m_runningRequests.end()) {
            httpClient = itr.value();
            m_runningRequests.erase(itr);
        }
    }
    if (httpClient)
        httpClient->terminate();
}

QnServerRestConnection::Request QnServerRestConnection::prepareRequest(HttpMethod method, const QUrl& url, const nx_http::StringType& contentType, const nx_http::StringType& messageBody)
{
    QnMediaServerResourcePtr server =  qnResPool->getResourceById<QnMediaServerResource>(m_serverId);
    if (!server)
        return Request();
    
    Request request;
    request.method = method;
    request.url = server->getApiUrl();
    request.url.setPath(url.path());
    request.url.setQuery(url.query());
    request.contentType = contentType;
    request.messageBody = messageBody;

    auto videoWallGuid = QnAppServerConnectionFactory::videowallGuid();
    if (!videoWallGuid.isNull())
        request.headers.emplace(Qn::VIDEOWALL_GUID_HEADER_NAME, videoWallGuid.toByteArray());
    request.headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    request.headers.emplace(Qn::EC2_RUNTIME_GUID_HEADER_NAME, qnCommon->runningInstanceGUID().toByteArray());
    request.headers.emplace(Qn::CUSTOM_USERNAME_HEADER_NAME, QnAppServerConnectionFactory::url().userName().toUtf8());
    request.headers.emplace("User-Agent", nx_http::userAgentString());

    QnRoute route = QnRouter::instance()->routeTo(server->getId());
    if (route.reverseConnect)
    {
        // no route exists. proxy via nearest server
        auto nearestServer = qnCommon->currentServer();
        if (!nearestServer)
            return Request();
        QUrl nearestUrl(server->getApiUrl());
        if (nearestServer->getId() == qnCommon->moduleGUID())
            request.url.setHost(lit("127.0.0.1"));
        else
            request.url.setHost(nearestUrl.host());
        request.url.setPort(nearestUrl.port());
    }
    else if (!route.addr.isNull())
    {
        request.url.setHost(route.addr.address.toString());
        request.url.setPort(route.addr.port);
    }

    QString user = QnAppServerConnectionFactory::url().userName();
    QString password = QnAppServerConnectionFactory::url().password();
    auto authType = nx_http::AsyncHttpClient::authBasicAndDigest;
    if (user.isEmpty() || password.isEmpty()) {
        if (QnUserResourcePtr admin = qnResPool->getAdministrator()) 
        {
            // if auth is not known, use admin hash
            user = admin->getName();
            password = QString::fromUtf8(admin->getDigest());
            authType = nx_http::AsyncHttpClient::authDigestWithPasswordHash;
        }
    }
    request.url.setUserName(user);
    request.url.setPassword(password);
    return request;
}
    
QnUuid QnServerRestConnection::sendRequest(const Request& request, HttpCompletionFunc callback)
{
    auto httpClientCaptured = nx_http::AsyncHttpClient::create();
    httpClientCaptured->setResponseReadTimeoutMs( ResponseReadTimeoutMs );
    httpClientCaptured->setSendTimeoutMs( TcpConnectTimeoutMs );
    httpClientCaptured->addRequestHeaders(request.headers);
    QnUuid requestId = QnUuid::createUuid();
    
    auto requestCompletionFunc = [requestId, callback, this, httpClientCaptured]
    ( nx_http::AsyncHttpClientPtr httpClient ) mutable
    {
        httpClientCaptured->disconnect( nullptr, (const char*)nullptr );
        httpClientCaptured.reset();

        QnMutexLocker lock(&m_mutex);
        auto itr = m_runningRequests.find(requestId);
        if (itr == m_runningRequests.end())
            return; // request canceled

        SystemError::ErrorCode systemError = SystemError::noError;
        nx_http::StatusCode::Value statusCode = nx_http::StatusCode::ok;
        nx_http::StringType contentType;
        nx_http::BufferType messageBody;
        
        if( httpClient->failed() ) {
            systemError = SystemError::connectionReset;
        }
        else {
            statusCode = (nx_http::StatusCode::Value) httpClient->response()->statusLine.statusCode;
            contentType = httpClient->contentType();
            messageBody = httpClient->fetchMessageBodyBuffer();
        }
        m_runningRequests.erase(itr); // free last reference to the object
        if (callback)
            callback(systemError, statusCode, contentType, messageBody);
    };
    
    connect(httpClientCaptured.get(), &nx_http::AsyncHttpClient::done, this, requestCompletionFunc, Qt::DirectConnection);

    QnMutexLocker lock(&m_mutex);
    bool result = false;
    if (request.method == HttpMethod::Get)
        result = httpClientCaptured->doGet(request.url);
    else if (request.method == HttpMethod::Post)
        result = httpClientCaptured->doPost(request.url, request.contentType, request.messageBody);
    else
        qWarning() << Q_FUNC_INFO << __LINE__ << "Unknown HTTP request type" << (int) request.method;

    if (result) {
        m_runningRequests.insert(requestId, httpClientCaptured);
        return requestId;
    }
    else {
        disconnect( httpClientCaptured.get(), nullptr, this, nullptr );
        return QnUuid();
    }
}
