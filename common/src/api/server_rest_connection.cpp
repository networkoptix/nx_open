#include <atomic>
#include "server_rest_connection.h"

#include <api/model/cloud_credentials_data.h>
#include <api/app_server_connection.h>
#include <api/helpers/empty_request_data.h>
#include <api/helpers/chunks_request_data.h>
#include <api/helpers/thumbnail_request_data.h>
#include <api/helpers/send_statistics_request_data.h>

#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx_ec/data/api_data.h>

#include <utils/common/model_functions.h>
#include <network/router.h>
#include <http/custom_headers.h>
#include <nx/network/http/httptypes.h>
#include <utils/common/delayed.h>

namespace {
    static const size_t ResponseReadTimeoutMs = 15 * 1000;
    static const size_t TcpConnectTimeoutMs   = 5 * 1000;
}

// --------------------------- public methods -------------------------------------------

namespace rest
{

ServerConnection::ServerConnection(const QnUuid& serverId):
    m_serverId(serverId)
{
}

ServerConnection::~ServerConnection()
{
    QMap<Handle, nx_http::AsyncHttpClientPtr> dataCopy;
    {
        QnMutexLocker lock(&m_mutex);
        std::swap(dataCopy, m_runningRequests);
    }
    for (auto value: dataCopy)
        value->terminate();
}

rest::Handle ServerConnection::cameraHistoryAsync(const QnChunksRequestData &request, Result<ec2::ApiCameraHistoryDataList>::type callback, QThread* targetThread)
{
    return executeGet(lit("/ec2/cameraHistory"), request.toParams(), callback, targetThread);
}

rest::Handle ServerConnection::cameraThumbnailAsync( const QnThumbnailRequestData &request, Result<QByteArray>::type callback, QThread* targetThread /*= 0*/ )
{
    return executeGet(lit("/ec2/cameraThumbnail"), request.toParams(), callback, targetThread);
}

Handle ServerConnection::getStatisticsSettingsAsync(Result<QByteArray>::type callback
    , QThread *targetThread)
{
    static const QnEmptyRequestData kEmptyParams = QnEmptyRequestData();
    return executeGet(lit("/ec2/statistics/settings"), kEmptyParams.toParams(), callback, targetThread);
}

Handle ServerConnection::sendStatisticsAsync(const QnSendStatisticsRequestData &request
    , PostCallback callback
    , QThread *targetThread)
{
    static const nx_http::StringType kJsonContentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);

    const nx_http::BufferType data = QJson::serialized(request.metricsList);
    if (data.isEmpty())
        return Handle();

    return executePost(lit("/ec2/statistics/send"), request.toParams()
        , kJsonContentType, data, callback, targetThread);
}

Handle ServerConnection::resetCloudSystemCredentials(
    Result<QnRestResult>::type callback,
    QThread* targetThread)
{
    CloudCredentialsData data;
    data.reset = true;

    return executePost(
        lit("/api/saveCloudSystemCredentials"),
        QnRequestParamList(),
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        QJson::serialized(std::move(data)),
        callback,
        targetThread);
}

Handle ServerConnection::saveCloudSystemCredentials(
    const QString& cloudSystemID,
    const QString& cloudAuthKey,
    const QString &cloudAccountName,
    Result<QnRestResult>::type callback,
    QThread* targetThread)
{
    CloudCredentialsData data;
    data.cloudSystemID = cloudSystemID;
    data.cloudAuthKey = cloudAuthKey;
    data.cloudAccountName = cloudAccountName;

    return executePost(
        lit("/api/saveCloudSystemCredentials"),
        QnRequestParamList(),
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        QJson::serialized(std::move(data)),
        callback,
        targetThread);
}

// --------------------------- private implementation -------------------------------------

QUrl ServerConnection::prepareUrl(const QString& path, const QnRequestParamList& params) const
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
    switch(format)
    {
    case Qn::JsonFormat:
        return QJson::deserialized(msgBody, T(), success);
    case Qn::UbjsonFormat:
        return QnUbjson::deserialized(msgBody, T(), success);
    default:
        if (success)
            *success = false;
        NX_ASSERT(0, Q_FUNC_INFO, "Unsupported data format");
        break;
    }
    return T();
}

template <typename ResultType>
Handle ServerConnection::executeGet(const QString& path, const QnRequestParamList& params, REST_CALLBACK(ResultType) callback, QThread* targetThread)
{
    Request request = prepareRequest(HttpMethod::Get, prepareUrl(path, params));
    return request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
}

template <typename ResultType>
Handle ServerConnection::executePost(const QString& path,
                                           const QnRequestParamList& params,
                                           const nx_http::StringType& contentType,
                                           const nx_http::StringType& messageBody,
                                           REST_CALLBACK(ResultType) callback,
                                           QThread* targetThread)
{
    Request request = prepareRequest(HttpMethod::Post, prepareUrl(path, params), contentType, messageBody);
    return request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
}

template <typename ResultType>
void invoke(REST_CALLBACK(ResultType) callback, QThread* targetThread, bool success, const Handle& id, const ResultType& result)
{
    if (targetThread)
        executeDelayed([callback, success, id, result] { callback(success, id, result); }, 0, targetThread);
    else
        callback(success, id, result);
}

template <typename ResultType>
Handle ServerConnection::executeRequest(const Request& request, REST_CALLBACK(ResultType) callback, QThread* targetThread)
{
    return sendRequest(request, [callback, targetThread] (Handle id, SystemError::ErrorCode osErrorCode, int statusCode, nx_http::StringType contentType, nx_http::BufferType msgBody)
    {
        bool success = false;
        ResultType result;
        if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok)
        {
            Qn::SerializationFormat format = Qn::serializationFormatFromHttpContentType(contentType);
            result = parseMessageBody<ResultType>(format, msgBody, &success);
        }
        invoke(callback, targetThread, success, id, result);
    });
}

Handle ServerConnection::executeRequest(const Request& request, REST_CALLBACK(QByteArray) callback, QThread* targetThread)
{
    QPointer<QThread> targetThreadGuard(targetThread);
    return sendRequest(request, [callback, targetThread, targetThreadGuard] (Handle id, SystemError::ErrorCode osErrorCode, int statusCode, nx_http::StringType contentType, nx_http::BufferType msgBody)
    {
        Q_UNUSED(contentType)

        bool success = (osErrorCode == SystemError::noError && statusCode >= nx_http::StatusCode::ok && statusCode <= nx_http::StatusCode::partialContent);

        if (targetThread && targetThreadGuard.isNull())
            return;

        invoke(callback, targetThread, success, id, msgBody);
    });
}

Handle ServerConnection::executeRequest(const Request& request, REST_CALLBACK(EmptyResponseType) callback, QThread* targetThread)
{
    QPointer<QThread> targetThreadGuard(targetThread);
    return sendRequest(request, [callback, targetThread, targetThreadGuard] (Handle id, SystemError::ErrorCode osErrorCode, int statusCode, nx_http::StringType, nx_http::BufferType)
    {
        bool success = (osErrorCode == SystemError::noError && statusCode >= nx_http::StatusCode::ok && statusCode <= nx_http::StatusCode::partialContent);

        if (targetThread && targetThreadGuard.isNull())
            return;

        invoke(callback, targetThread, success, id, EmptyResponseType());
    });
}

void ServerConnection::cancelRequest(const Handle& requestId)
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

ServerConnection::Request ServerConnection::prepareRequest(HttpMethod method, const QUrl& url, const nx_http::StringType& contentType, const nx_http::StringType& messageBody)
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
    if (user.isEmpty() || password.isEmpty()) {
        if (QnUserResourcePtr admin = qnResPool->getAdministrator())
        {
            // if auth is not known, use admin hash
            user = admin->getName();
            password = QString::fromUtf8(admin->getDigest());
            request.authType = nx_http::AsyncHttpClient::authDigestWithPasswordHash;
        }
    }
    request.url.setUserName(user);
    request.url.setPassword(password);
    return request;
}

Handle ServerConnection::sendRequest(const Request& request, HttpCompletionFunc callback)
{
    static std::atomic<Handle> requestNum;
    Handle requestId = ++requestNum;

    auto httpClientCaptured = nx_http::AsyncHttpClient::create();
    httpClientCaptured->setResponseReadTimeoutMs( ResponseReadTimeoutMs );
    httpClientCaptured->setSendTimeoutMs( TcpConnectTimeoutMs );
    httpClientCaptured->addRequestHeaders(request.headers);
    httpClientCaptured->setAuthType(request.authType);

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
        lock.unlock();
        if (callback)
            callback(requestId, systemError, statusCode, contentType, messageBody);
        lock.relock();
        m_runningRequests.remove(requestId); // free last reference to the object
    };

    connect(httpClientCaptured.get(), &nx_http::AsyncHttpClient::done, this, requestCompletionFunc, Qt::DirectConnection);

    QnMutexLocker lock(&m_mutex);
    if (request.method == HttpMethod::Get)
    {
        httpClientCaptured->doGet(request.url);
    }
    else if (request.method == HttpMethod::Post)
    {
        httpClientCaptured->doPost(request.url, request.contentType, request.messageBody);
    }
    else
    {
        disconnect(httpClientCaptured.get(), nullptr, this, nullptr);
        return Handle();
    }
    m_runningRequests.insert(requestId, httpClientCaptured);
    return requestId;
}


} // namespace rest
