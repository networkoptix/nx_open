#include "server_rest_connection.h"

#include <atomic>

#include <QtCore/QElapsedTimer>

#include <api/model/password_data.h>
#include <api/model/cloud_credentials_data.h>
#include <api/model/update_information_reply.h>
#include <api/app_server_connection.h>
#include <api/helpers/empty_request_data.h>
#include <api/helpers/chunks_request_data.h>
#include <api/helpers/thumbnail_request_data.h>
#include <api/helpers/send_statistics_request_data.h>
#include <api/helpers/event_log_request_data.h>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx_ec/data/api_data.h>

#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/rule.h>
#include <nx/fusion/model_functions.h>
#include <network/router.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>
#include <common/common_module.h>

#include <nx/utils/random.h>
#include <nx/utils/log/log.h>

namespace {

static const size_t ResponseReadTimeoutMs = 15 * 1000;
static const size_t TcpConnectTimeoutMs = 5 * 1000;

void trace(const QString& serverId, int handle, const QString& message)
{
    static const nx::utils::log::Tag kTag(typeid(rest::ServerConnection));
    NX_VERBOSE(kTag) << lm("%1 <%2>: %3").args(serverId, handle, message);
}

} // namepspace

// --------------------------- public methods -------------------------------------------

namespace rest
{

ServerConnection::ServerConnection(
    QnCommonModule* commonModule,
    const QnUuid& serverId)
    :
    QObject(),
    QnCommonModuleAware(commonModule),
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

rest::Handle ServerConnection::twoWayAudioCommand(const QString& sourceId,
    const QnUuid& cameraId,
    bool start,
    GetCallback callback,
    QThread* targetThread /*= 0*/)
{
    QnRequestParamList params;
    params.insert(lit("clientId"), sourceId);
    params.insert(lit("resourceId"),    cameraId.toString());
    params.insert(lit("action"),        start ? lit("start") : lit("stop"));
    return executeGet(lit("/api/transmitAudio"), params, callback, targetThread);
}

rest::Handle ServerConnection::softwareTriggerCommand(const QnUuid& cameraId, const QString& triggerId,
    nx::vms::event::EventState toggleState, GetCallback callback, QThread* targetThread)
{
    QnRequestParamList params;
    params.insert(lit("timestamp"), lit("%1").arg(qnSyncTime->currentMSecsSinceEpoch()));
    params.insert(lit("event_type"), QnLexical::serialized(nx::vms::event::softwareTriggerEvent));
    params.insert(lit("inputPortId"), triggerId);
    params.insert(lit("eventResourceId"), cameraId.toString());
    if (toggleState != nx::vms::event::EventState::undefined)
        params.insert(lit("state"), QnLexical::serialized(toggleState));
    return executeGet(lit("/api/createEvent"), params, callback, targetThread);
}

QnMediaServerResourcePtr ServerConnection::getServerWithInternetAccess() const
{
    QnMediaServerResourcePtr server =
        commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->remoteGUID());
    if (!server)
        return QnMediaServerResourcePtr(); //< something wrong. No current server available

    if (server->getServerFlags().testFlag(Qn::SF_HasPublicIP))
        return server;

    // Current server doesn't have internet access. Try to find another one
    for (const auto server: commonModule()->resourcePool()->getAllServers(Qn::Online))
    {
        if (server->getServerFlags().testFlag(Qn::SF_HasPublicIP))
            return server;
    }
    return QnMediaServerResourcePtr(); //< no internet access found
}

void ServerConnection::trace(int handle, const QString& message) const
{
    ::trace(m_serverId.toString(), handle, message);
}

Handle ServerConnection::getStatisticsSettingsAsync(
    Result<QByteArray>::type callback,
    QThread *targetThread)
{
    QnEmptyRequestData emptyRequest;
    emptyRequest.format = Qn::SerializationFormat::UbjsonFormat;
    static const auto path = lit("/ec2/statistics/settings");

    QnMediaServerResourcePtr server = getServerWithInternetAccess();
    if (!server)
        return Handle(); //< can't process request now. No internet access

    nx_http::ClientPool::Request request = prepareRequest(
        nx_http::Method::get, prepareUrl(path, emptyRequest.toParams()));
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
        nx_http::Method::post,
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
        auto admin = commonModule()->resourcePool()->getAdministrator();
        NX_ASSERT(admin);
        if (!admin)
            return Handle();
        data = PasswordData::calculateHashes(admin->getName(), resetAdminPassword, false);
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

Handle ServerConnection::addFileDownload(
    const QString& fileName,
    int size,
    const QByteArray& md5,
    const QUrl& url,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        lit("/api/downloads/%1").arg(fileName),
        QnRequestParamList{
            {lit("size"), QString::number(size)},
            {lit("md5"), QString::fromUtf8(md5)},
            {lit("url"), url.toString()}},
        QByteArray(),
        QByteArray(),
        callback,
        targetThread);
}

Handle ServerConnection::removeFileDownload(
    const QString& fileName,
    bool deleteData,
    GetCallback callback,
    QThread* targetThread)
{
    return executeDelete(
        lit("/api/downloads/%1").arg(fileName),
        QnRequestParamList{{lit("deleteData"), QnLexical::serialized(deleteData)}},
        callback,
        targetThread);
}

Handle ServerConnection::fileChunkChecksums(
    const QString& fileName,
    GetCallback callback,
    QThread* targetThread)
{
    return executeGet(
        lit("/api/downloads/%1/checksums").arg(fileName),
        QnRequestParamList(),
        callback,
        targetThread);
}

Handle ServerConnection::downloadFileChunk(
    const QString& fileName,
    int index,
    Result<QByteArray>::type callback,
    QThread* targetThread)
{
    return executeGet(
        lit("/api/downloads/%1/chunks/%2").arg(fileName).arg(index),
        QnRequestParamList(),
        callback,
        targetThread);
}

Handle ServerConnection::downloadFileChunkFromInternet(
    const QString& fileName,
    const nx::utils::Url& url,
    int chunkIndex,
    int chunkSize,
    Result<QByteArray>::type callback,
    QThread* targetThread)
{
    return executeGet(
        lit("/api/downloads/%1/chunks/%2").arg(fileName).arg(chunkIndex),
        QnRequestParamList{
            {lit("url"), url.toString()},
            {lit("chunkSize"), QString::number(chunkSize)}},
        callback,
        targetThread);
}

Handle ServerConnection::uploadFileChunk(
    const QString& fileName,
    int index,
    const QByteArray& data,
    GetCallback callback,
    QThread* targetThread)
{
    return executePut(
        lit("/api/downloads/%1/chunks/%2").arg(fileName).arg(index),
        QnRequestParamList(),
        "application/octet-stream",
        data,
        callback,
        targetThread);
}

Handle ServerConnection::downloadsStatus(
    GetCallback callback,
    QThread* targetThread)
{
    return executeGet(
        lit("/api/downloads/status"),
        QnRequestParamList(),
        callback,
        targetThread);
}

Handle ServerConnection::fileDownloadStatus(
    const QString& fileName,
    GetCallback callback,
    QThread* targetThread)
{
    return executeGet(
        lit("/api/downloads/%1/status").arg(fileName),
        QnRequestParamList(),
        callback,
        targetThread);
}

Handle ServerConnection::getTimeOfServersAsync(
    Result<MultiServerTimeData>::type callback,
    QThread* targetThread)
{
    return executeGet(lit("/ec2/getTimeOfServers"), QnRequestParamList(), callback, targetThread);
}

rest::Handle ServerConnection::testEventRule(const QnUuid& ruleId,
    nx::vms::event::EventState toggleState,
    GetCallback callback,
    QThread* targetThread)
{
    auto manager = commonModule()->eventRuleManager();
    NX_ASSERT(manager);
    auto rule = manager->rule(ruleId);
    if (!rule)
        return 0;

    QnRequestParamList params;
    params.insert(lit("event_type"), QnLexical::serialized(rule->eventType()));
    params.insert(lit("timestamp"), lit("%1").arg(qnSyncTime->currentMSecsSinceEpoch()));

    if (rule->eventResources().size() > 0)
    {
        auto randomResource = nx::utils::random::choice(rule->eventResources());
        params.insert(lit("eventResourceId"), randomResource.toString());
    }
    else if (nx::vms::event::isSourceCameraRequired(rule->eventType())
        || rule->eventType() >= nx::vms::event::userDefinedEvent)
    {
        auto randomCamera = nx::utils::random::choice(
            commonModule()->resourcePool()->getAllCameras(QnResourcePtr(), true));
        params.insert(lit("eventResourceId"), randomCamera->getId().toString());
    }
    else if (nx::vms::event::isSourceServerRequired(rule->eventType()))
    {
        auto randomServer = nx::utils::random::choice(
            commonModule()->resourcePool()->getAllServers(Qn::Online));
        params.insert(lit("eventResourceId"), randomServer->getId().toString());
    }

    if (toggleState != nx::vms::event::EventState::undefined)
        params.insert(lit("state"), QnLexical::serialized(toggleState));

    params.insert(lit("inputPortId"), rule->eventParams().inputPortId);
    params.insert(lit("source"), rule->eventParams().resourceName);
    params.insert(lit("caption"), rule->eventParams().caption);
    params.insert(lit("description"), rule->eventParams().description);
    params.insert(lit("metadata"), QString::fromUtf8(
        QJson::serialized(rule->eventParams().metadata)));

    return executeGet(lit("/api/createEvent"), params, callback, targetThread);
}

Handle ServerConnection::getEvents(QnEventLogRequestData request,
    Result<EventLogData>::type callback,
    QThread *targetThread)
{
    request.format = Qn::SerializationFormat::UbjsonFormat;
    return executeGet(lit("/api/getEvents"), request.toParams(), callback, targetThread);
}

Handle ServerConnection::changeCameraPassword(
    const QnUuid& id,
    const QAuthenticator& auth,
    Result<QnRestResult>::type callback,
    QThread* targetThread)
{
    const auto camera = resourcePool()->getResourceById(id);
    if (!camera || camera->getParentId().isNull())
        return Handle();

    CameraPasswordData data;
    data.cameraId = id.toString();
    data.user = auth.user();
    data.password = auth.password();

    QnEmptyRequestData params;
    params.format = Qn::SerializationFormat::UbjsonFormat;
    nx_http::ClientPool::Request request = prepareRequest(
        nx_http::Method::post,
        prepareUrl(lit("/api/changeCameraPassword"), params.toParams()));
    request.messageBody = QJson::serialized(std::move(data));
    request.headers.emplace(Qn::SERVER_GUID_HEADER_NAME, camera->getParentId().toByteArray());

    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    trace(handle, request.url.toString());
    return handle;
}

// --------------------------- private implementation -------------------------------------

QUrl ServerConnection::prepareUrl(const QString& path, const QnRequestParamList& params) const
{
    QUrl result;
    result.setPath(path);
    QUrlQuery q;
    for (const auto& param: params)
        q.addQueryItem(param.first, QString::fromUtf8(QUrl::toPercentEncoding(param.second)));
    result.setQuery(q);
    return result;
}

template<typename T,
    typename std::enable_if<std::is_base_of<RestResultWithDataBase, T>::value>::type* = nullptr>
T parseMessageBody(
    const Qn::SerializationFormat& format,
    const nx_http::BufferType& msgBody,
    bool* success)
{
     switch (format)
     {
         case Qn::JsonFormat:
         {
              auto restResult = QJson::deserialized(msgBody, QnJsonRestResult(), success);
              return T(restResult, restResult.deserialized<decltype(T::data)>());
         }
         case Qn::UbjsonFormat:
         {
             auto restResult = QnUbjson::deserialized(msgBody, QnUbjsonRestResult(), success);
             return T(restResult, restResult.deserialized<decltype(T::data)>());
         }
         default:
             if (success)
                 *success = false;
             NX_ASSERT(0, Q_FUNC_INFO, "Unsupported data format");
             break;
     }
    return T();
}

template<typename T,
    typename std::enable_if<!std::is_base_of<RestResultWithDataBase, T>::value>::type* = nullptr>
T parseMessageBody(
    const Qn::SerializationFormat& format,
    const nx_http::BufferType& msgBody,
    bool* success)
{
    switch (format)
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
Handle ServerConnection::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    Callback<ResultType> callback,
    QThread* targetThread)
{
    auto request = prepareRequest(nx_http::Method::get, prepareUrl(path, params));
    auto handle = request.isValid()
        ? executeRequest(request, callback, targetThread)
        : Handle();

    trace(handle, path);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const nx_http::StringType& contentType,
    const nx_http::StringType& messageBody,
    Callback<ResultType> callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx_http::Method::post, prepareUrl(path, params), contentType, messageBody);
    auto handle = request.isValid()
        ? executeRequest(request, callback, targetThread)
        : Handle();

    trace(handle, path);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executePut(
    const QString& path,
    const QnRequestParamList& params,
    const nx_http::StringType& contentType,
    const nx_http::StringType& messageBody,
    Callback<ResultType> callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx_http::Method::put, prepareUrl(path, params), contentType, messageBody);
    auto handle = request.isValid()
        ? executeRequest(request, callback, targetThread)
        : Handle();

    trace(handle, path);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executeDelete(
    const QString& path,
    const QnRequestParamList& params,
    Callback<ResultType> callback,
    QThread* targetThread)
{
    auto request = prepareRequest(nx_http::Method::delete_, prepareUrl(path, params));
    auto handle = request.isValid()
        ? executeRequest(request, callback, targetThread)
        : Handle();

    trace(handle, path);
    return handle;
}

template <typename ResultType>
void invoke(Callback<ResultType> callback,
            QThread* targetThread,
            bool success,
            const Handle& id,
            ResultType result,
            const QString &serverId,
            const QElapsedTimer& elapsed
            )
{
    if (success)
        trace(serverId, id, lit("Reply success for %1ms").arg(elapsed.elapsed()));
    else
        trace(serverId, id, lit("Reply failed for %1ms").arg(elapsed.elapsed()));

    if (targetThread)
    {
         auto ptr = std::make_shared<ResultType>(std::move(result));
         executeDelayed([callback, success, id, ptr]() mutable
             {
                 callback(success, id, std::move(*ptr));
             },
             0,
             targetThread);
    }
    else
    {
        callback(success, id, std::move(result));
    }
}

template <typename ResultType>
Handle ServerConnection::executeRequest(
    const nx_http::ClientPool::Request& request,
    Callback<ResultType> callback,
    QThread* targetThread)
{
    if (callback)
    {
        const QString serverId = m_serverId.toString();
        QElapsedTimer timer;
        timer.start();

        return sendRequest(request,
            [callback, targetThread, serverId, timer]
            (Handle id,
                SystemError::ErrorCode osErrorCode,
                int statusCode,
                nx_http::StringType contentType,
                nx_http::BufferType msgBody)
            {
                bool success = false;
                if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok)
                {
                    const auto format = Qn::serializationFormatFromHttpContentType(contentType);
                    auto result = parseMessageBody<ResultType>(format, msgBody, &success);
                    invoke(callback,
                        targetThread,
                        success,
                        id,
                        std::move(result),
                        serverId,
                        timer);
                }
                else
                {
                    invoke(callback, targetThread, success, id, ResultType(), serverId, timer);
                }
            });
    }

    return sendRequest(request);
}

Handle ServerConnection::executeRequest(
    const nx_http::ClientPool::Request& request,
    Callback<QByteArray> callback,
    QThread* targetThread)
{
    if (callback)
    {
        const QString serverId = m_serverId.toString();
        QElapsedTimer timer;
        timer.start();

        QPointer<QThread> targetThreadGuard(targetThread);
        return sendRequest(request,
            [callback, targetThread, targetThreadGuard, serverId, timer]
            (Handle id, SystemError::ErrorCode osErrorCode, int statusCode, nx_http::StringType contentType, nx_http::BufferType msgBody)
            {
                Q_UNUSED(contentType)
                bool success = (osErrorCode == SystemError::noError
                    && statusCode >= nx_http::StatusCode::ok
                    && statusCode <= nx_http::StatusCode::partialContent);

                if (targetThread && targetThreadGuard.isNull())
                    return;

                invoke(callback, targetThread, success, id, std::move(msgBody), serverId, timer);
            });
    }

    return sendRequest(request);
}

Handle ServerConnection::executeRequest(
    const nx_http::ClientPool::Request& request,
    Callback<EmptyResponseType> callback,
    QThread* targetThread)
{
    if (callback)
    {
        const QString serverId = m_serverId.toString();
        QElapsedTimer timer;
        timer.start();

        QPointer<QThread> targetThreadGuard(targetThread);
        return sendRequest(request,
            [callback, targetThread, targetThreadGuard, serverId, timer]
            (Handle id, SystemError::ErrorCode osErrorCode, int statusCode, nx_http::StringType, nx_http::BufferType)
            {
                bool success = (osErrorCode == SystemError::noError
                    && statusCode >= nx_http::StatusCode::ok
                    && statusCode <= nx_http::StatusCode::partialContent);

                if (targetThread && targetThreadGuard.isNull())
                    return;

                invoke(callback, targetThread, success, id, EmptyResponseType(), serverId, timer);
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
    auto resPool = commonModule()->resourcePool();
    const auto server = resPool->getResourceById<QnMediaServerResource>(m_serverId);
    if (!server)
        return nx_http::ClientPool::Request();
    const auto connection = commonModule()->ec2Connection();
    if (!connection)
        return nx_http::ClientPool::Request();

    nx_http::ClientPool::Request request;
    request.method = method;
    request.url = server->getApiUrl();
    request.url.setPath(url.path());
    request.url.setQuery(url.query());
    request.contentType = contentType;
    request.messageBody = messageBody;

    QString user = connection->connectionInfo().ecUrl.userName();
    QString password = connection->connectionInfo().ecUrl.password();
    if (user.isEmpty() || password.isEmpty())
    {
        // if auth is not known, use server auth key
        user = server->getId().toString();
        password = server->getAuthKey();
    }
    auto videoWallGuid = commonModule()->videowallGuid();
    if (!videoWallGuid.isNull())
        request.headers.emplace(Qn::VIDEOWALL_GUID_HEADER_NAME, videoWallGuid.toByteArray());
    request.headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    request.headers.emplace(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME, commonModule()->runningInstanceGUID().toByteArray());
    request.headers.emplace(Qn::CUSTOM_USERNAME_HEADER_NAME, user.toLower().toUtf8());

    const auto& router = commonModule()->router();
    QnRoute route = router->routeTo(server->getId());
    if (route.reverseConnect)
    {
        // no route exists. proxy via nearest server
        auto nearestServer = commonModule()->currentServer();
        if (!nearestServer)
            return nx_http::ClientPool::Request();
        nx::utils::Url nearestUrl(server->getApiUrl());
        if (nearestServer->getId() == commonModule()->moduleGUID())
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

Handle ServerConnection::sendRequest(
    const nx_http::ClientPool::Request& request,
    HttpCompletionFunc callback)
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
