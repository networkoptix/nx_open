#include <atomic>
#include "server_rest_connection.h"

#include <api/model/password_data.h>
#include <api/model/cloud_credentials_data.h>
#include <api/model/update_information_reply.h>
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

#include <nx/fusion/model_functions.h>
#include <network/router.h>
#include <http/custom_headers.h>
#include <nx/network/http/httptypes.h>
#include <utils/common/delayed.h>
#include <nx/utils/log/log.h>

namespace {
    static const size_t ResponseReadTimeoutMs = 15 * 1000;
    static const size_t TcpConnectTimeoutMs   = 5 * 1000;

    void trace(int handle, const QString& message)
    {
        NX_LOG(lit("QnMediaServerConnection %1: %2").arg(handle).arg(message), cl_logDEBUG1);
    }
}

// --------------------------- public methods -------------------------------------------

namespace rest
{

ServerConnection::ServerConnection(const QnUuid& serverId):
    m_serverId(serverId)
{
    auto httpPool = nx_http::ClientPool::instance();
    Qn::directConnect(
        httpPool, &nx_http::ClientPool::done,
        this, &ServerConnection::onHttpClientDone);
}

ServerConnection::~ServerConnection()
{
    directDisconnectAll();
    auto httpPool = nx_http::ClientPool::instance();
    for (const auto& handle : m_runningRequests.keys())
        httpPool->terminate(handle);
}

rest::Handle ServerConnection::cameraHistoryAsync(const QnChunksRequestData &request, Result<ec2::ApiCameraHistoryDataList>::type callback, QThread* targetThread)
{
    return executeGet(lit("/ec2/cameraHistory"), request.toParams(), callback, targetThread);
}

rest::Handle ServerConnection::cameraThumbnailAsync( const QnThumbnailRequestData &request, Result<QByteArray>::type callback, QThread* targetThread /*= 0*/ )
{
    return executeGet(lit("/ec2/cameraThumbnail"), request.toParams(), callback, targetThread);
}

rest::Handle ServerConnection::twoWayAudioCommand(const QnUuid& cameraId, bool start, GetCallback callback, QThread* targetThread /*= 0*/)
{
    QnRequestParamList params;
    params.insert(lit("clientId"),      qnCommon->moduleGUID().toString());
    params.insert(lit("resourceId"),    cameraId.toString());
    params.insert(lit("action"),        start ? lit("start") : lit("stop"));
    return executeGet(lit("/api/transmitAudio"), params, callback, targetThread);
}

QnMediaServerResourcePtr ServerConnection::getServerWithInternetAccess() const
{
    QnMediaServerResourcePtr server =
        qnResPool->getResourceById<QnMediaServerResource>(qnCommon->remoteGUID());
    if (!server)
        return QnMediaServerResourcePtr(); //< something wrong. No current server available

    if (server->getServerFlags().testFlag(Qn::SF_HasPublicIP))
        return server;

    // Current server doesn't have internet access. Try to find another one
    for (const auto server: qnResPool->getAllServers(Qn::Online))
    {
        if (server->getServerFlags().testFlag(Qn::SF_HasPublicIP))
            return server;
    }
    return QnMediaServerResourcePtr(); //< no internet access found
}

Handle ServerConnection::getStatisticsSettingsAsync(
    Result<QByteArray>::type callback,
    QThread *targetThread)
{
    static const QnEmptyRequestData kEmptyParams = QnEmptyRequestData();
    static const auto path = lit("/ec2/statistics/settings");

    QnMediaServerResourcePtr server = getServerWithInternetAccess();
    if (!server)
        return Handle(); //< can't process request now. No internet access

    nx_http::ClientPool::Request request = prepareRequest(nx_http::Method::GET, prepareUrl(path, kEmptyParams.toParams()));
    nx_http::HttpHeader header(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    nx_http::insertOrReplaceHeader(&request.headers, header);
    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    trace(handle, path);
    return handle;
}

Handle ServerConnection::sendStatisticsAsync(
    const QnSendStatisticsRequestData& statisticsData,
    PostCallback callback,
    QThread *targetThread)
{
    static const nx_http::StringType kJsonContentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
    static const auto path = lit("/ec2/statistics/send");

    auto server = getServerWithInternetAccess();
    if (!server)
        return Handle(); //< can't process request now. No internet access

    const nx_http::BufferType data = QJson::serialized(statisticsData.metricsList);
    if (data.isEmpty())
        return Handle();

    nx_http::ClientPool::Request request = prepareRequest(
        nx_http::Method::POST,
        prepareUrl(path, statisticsData.toParams()),
        kJsonContentType,
        data);
    nx_http::HttpHeader header(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    nx_http::insertOrReplaceHeader(&request.headers, header);
    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    trace(handle, path);
    return handle;
}

Handle ServerConnection::detachSystemFromCloud(
    const QString& resetAdminPassword,
    Result<QnRestResult>::type callback,
    QThread* targetThread)
{
    PasswordData data;
    if (!resetAdminPassword.isEmpty())
    {
        auto admin = qnResPool->getAdministrator();
        NX_ASSERT(admin);
        if (!admin)
            return Handle();
        data = PasswordData::calculateHashes(admin->getName(), resetAdminPassword);
    }

    return executePost(
        lit("/api/detachFromCloud"),
        QnRequestParamList(),
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        QJson::serialized(std::move(data)),
        callback,
        targetThread);
}

Handle ServerConnection::saveCloudSystemCredentials(
    const QString& cloudSystemId,
    const QString& cloudAuthKey,
    const QString &cloudAccountName,
    Result<QnRestResult>::type callback,
    QThread* targetThread)
{
    CloudCredentialsData data;
    data.cloudSystemID = cloudSystemId;
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

Handle ServerConnection::startLiteClient(GetCallback callback, QThread* targetThread)
{
    QnRequestParamList params;
    params.append({lit("startCamerasMode"), lit("true")});
    return executeGet(lit("/api/startLiteClient"), params, callback, targetThread);
}

Handle ServerConnection::getFreeSpaceForUpdateFiles(
    Result<QnUpdateFreeSpaceReply>::type callback,
    QThread* targetThread)
{
    return executeGet(
        lit("/ec2/updateInformation/freeSpaceForUpdateFiles"),
        QnRequestParamList(),
        callback,
        targetThread);
}

Handle ServerConnection::checkCloudHost(
    Result<QnCloudHostCheckReply>::type callback,
    QThread* targetThread)
{
    return executeGet(
        lit("/ec2/updateInformation/checkCloudHost"),
        QnRequestParamList(),
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
    nx_http::ClientPool::Request request = prepareRequest(nx_http::Method::GET, prepareUrl(path, params));
    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    trace(handle, path);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executePost(const QString& path,
                                           const QnRequestParamList& params,
                                           const nx_http::StringType& contentType,
                                           const nx_http::StringType& messageBody,
                                           REST_CALLBACK(ResultType) callback,
                                           QThread* targetThread)
{
    nx_http::ClientPool::Request request = prepareRequest(nx_http::Method::POST, prepareUrl(path, params), contentType, messageBody);
    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    trace(handle, path);
    return handle;
}

template <typename ResultType>
void invoke(REST_CALLBACK(ResultType) callback, QThread* targetThread, bool success, const Handle& id, const ResultType& result)
{
    if (targetThread)
    {
        executeDelayed(
        [callback, success, id, result]
        {
            trace(id, lit("Reply"));
            callback(success, id, result);
        }, 0, targetThread);
    }
    else
    {
        trace(id, lit("Reply"));
        callback(success, id, result);
    }
}

template <typename ResultType>
Handle ServerConnection::executeRequest(const nx_http::ClientPool::Request& request, REST_CALLBACK(ResultType) callback, QThread* targetThread)
{
    if (callback)
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

    return sendRequest(request);
}

Handle ServerConnection::executeRequest(const nx_http::ClientPool::Request& request, REST_CALLBACK(QByteArray) callback, QThread* targetThread)
{
    if (callback)
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

    return sendRequest(request);
}

Handle ServerConnection::executeRequest(const nx_http::ClientPool::Request& request, REST_CALLBACK(EmptyResponseType) callback, QThread* targetThread)
{
    if (callback)
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

    return sendRequest(request);
}

void ServerConnection::cancelRequest(const Handle& requestId)
{
    nx_http::ClientPool::instance()->terminate(requestId);
    QnMutexLocker lock(&m_mutex);
    m_runningRequests.remove(requestId);
}

nx_http::ClientPool::Request ServerConnection::prepareRequest(
    nx_http::Method::ValueType method,
    const QUrl& url,
    const nx_http::StringType& contentType,
    const nx_http::StringType& messageBody)
{
    const auto server =  qnResPool->getResourceById<QnMediaServerResource>(m_serverId);
    if (!server)
        return nx_http::ClientPool::Request();

    nx_http::ClientPool::Request request;
    request.method = method;
    request.url = server->getApiUrl();
    request.url.setPath(url.path());
    request.url.setQuery(url.query());
    request.contentType = contentType;
    request.messageBody = messageBody;

    QString user = QnAppServerConnectionFactory::url().userName();
    QString password = QnAppServerConnectionFactory::url().password();
    if (user.isEmpty() || password.isEmpty())
    {
        // if auth is not known, use server auth key
        user = server->getId().toString();
        password = server->getAuthKey();
    }
    auto videoWallGuid = QnAppServerConnectionFactory::videowallGuid();
    if (!videoWallGuid.isNull())
        request.headers.emplace(Qn::VIDEOWALL_GUID_HEADER_NAME, videoWallGuid.toByteArray());
    request.headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    request.headers.emplace(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME, qnCommon->runningInstanceGUID().toByteArray());
    request.headers.emplace(Qn::CUSTOM_USERNAME_HEADER_NAME, user.toLower().toUtf8());

    QnRoute route = QnRouter::instance()->routeTo(server->getId());
    if (route.reverseConnect)
    {
        // no route exists. proxy via nearest server
        auto nearestServer = qnCommon->currentServer();
        if (!nearestServer)
            return nx_http::ClientPool::Request();
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


    request.url.setUserName(user);
    request.url.setPassword(password);

    return request;
}

Handle ServerConnection::sendRequest(const nx_http::ClientPool::Request& request, HttpCompletionFunc callback)
{
    auto httpPool = nx_http::ClientPool::instance();
    QnMutexLocker lock(&m_mutex);
    Handle requestId = httpPool->sendRequest(request);
    m_runningRequests.insert(requestId, callback);
    return requestId;
}

void ServerConnection::onHttpClientDone(int requestId, nx_http::AsyncHttpClientPtr httpClient)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_runningRequests.find(requestId);
    if (itr == m_runningRequests.end())
        return; // request canceled
    HttpCompletionFunc callback = itr.value();
    m_runningRequests.remove(requestId);

    SystemError::ErrorCode systemError = SystemError::noError;
    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::ok;
    nx_http::StringType contentType;
    nx_http::BufferType messageBody;

    if (httpClient->failed()) {
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
};

} // namespace rest
