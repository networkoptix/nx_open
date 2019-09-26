#include "server_rest_connection.h"

#include <atomic>

#include <QtCore/QElapsedTimer>

#include <api/model/detach_from_cloud_data.h>
#include <api/model/password_data.h>
#include <api/model/cloud_credentials_data.h>
#include <api/model/update_information_reply.h>

#include <api/app_server_connection.h>
#include <api/helpers/empty_request_data.h>
#include <api/helpers/chunks_request_data.h>
#include <api/helpers/thumbnail_request_data.h>
#include <api/helpers/send_statistics_request_data.h>
#include <api/helpers/event_log_request_data.h>
#include <api/helpers/event_log_multiserver_request_data.h>

#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/router.h>
#include <utils/common/delayed.h>
#include <utils/common/ldap.h>
#include <utils/common/synctime.h>

#include <nx/api/mediaserver/image_request.h>
#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/compressed_time_functions.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/random.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data_fwd.h>
#include <nx/vms/api/data/email_settings_data.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/rule.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>

#include <common/static_common_module.h>

using namespace nx;

namespace {

void trace(const QString& serverId, rest::Handle handle, const QString& message)
{
    static const nx::utils::log::Tag kTag(typeid(rest::ServerConnection));
    NX_VERBOSE(kTag) << lm("%1 <%2>: %3").args(serverId, handle, message);
}

/** Response deserialization for RestResultWithDataBase objects. */
template<typename T,
    typename std::enable_if<std::is_base_of<rest::RestResultWithDataBase, T>::value>::type* = nullptr>
T parseMessageBody(
    const Qn::SerializationFormat& format,
    const nx::network::http::BufferType& msgBody,
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
             NX_ASSERT(0, "Unsupported data format");
             break;
     }
    return T();
}

/** Response deserialization for the objects being not inherited from RestResultWithDataBase. */
template<typename T,
    typename std::enable_if<!std::is_base_of<rest::RestResultWithDataBase, T>::value>::type* = nullptr>
T parseMessageBody(
    const Qn::SerializationFormat& format,
    const nx::network::http::BufferType& msgBody,
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
            NX_ASSERT(0, "Unsupported data format");
            break;
    }
    return T();
}

// Invokes callback in appropriate thread
void invoke(network::http::ClientPool::ContextPtr context,
    std::function<void ()> callback,
    bool success,
    const QString &serverId
)
{
    NX_ASSERT(context);
    if (!context)
        return;
    /*
     * TODO: It can be moved to ClientPool::context
     * `targetThread` is also stored there.
     * `serverId` is still missing
     */
    auto elapsedMs = context->getTimeElapsed().count();
    if (success)
        trace(serverId, context->handle, lit("Reply success for %1ms").arg(elapsedMs));
    else
        trace(serverId, context->handle, lit("Reply failed for %1ms").arg(elapsedMs));

    if (auto thread = context->getTargetThread())
        executeLaterInThread(callback, thread);
    else
        callback();
}

} // namespace

// --------------------------- public methods -------------------------------------------

namespace rest
{

ServerConnection::ServerConnection(
    QnCommonModule* commonModule,
    const QnUuid& serverId,
    const nx::utils::Url& directUrl)
    :
    QObject(),
    QnCommonModuleAware(commonModule),
    m_serverId(serverId),
    m_directUrl(directUrl),
    m_logTag(QStringLiteral("%1 [%2]").arg(::toString(this), serverId.toString()))
{
    Qn::directConnect(httpClientPool(), &nx::network::http::ClientPool::requestIsDone, this,
        [this](nx::network::http::ClientPool::ContextPtr context)
        {
            auto requestId = context->handle;
            QnMutexLocker lock(&m_mutex);
            auto itr = m_runningRequests.find(requestId);
            if (itr == m_runningRequests.end())
                return; //< Request cancelled.
            m_runningRequests.remove(requestId);
        });
}

ServerConnection::~ServerConnection()
{
    directDisconnectAll();
    for (const auto& handle: m_runningRequests)
        httpClientPool()->terminate(handle);
}

rest::Handle ServerConnection::cameraHistoryAsync(
    const QnChunksRequestData& request,
    Result<nx::vms::api::CameraHistoryDataList>::type callback,
    QThread* targetThread)
{
    return executeGet(lit("/ec2/cameraHistory"), request.toParams(), callback, targetThread);
}

Handle ServerConnection::getServerLocalTime(Result<QnJsonRestResult>::type callback,
    QThread* targetThread)
{
    QnRequestParamList params{{lit("local"), QnLexical::serialized(true)}};
    return executeGet(lit("/api/gettime"), params, callback, targetThread);
}

rest::Handle ServerConnection::cameraThumbnailAsync(const nx::api::CameraImageRequest& request,
    Result<QByteArray>::type callback,
    QThread* targetThread)
{
    QnThumbnailRequestData data;
    data.request = request;
    data.format = Qn::UbjsonFormat;

    return executeGet(lit("/ec2/cameraThumbnail"), data.toParams(), callback, targetThread);
}

Handle ServerConnection::createGenericEvent(
    const QString& source,
    const QString& caption,
    const QString& description,
    const nx::vms::event::EventMetaData& metadata,
    nx::vms::api::EventState toggleState,
    GetCallback callback,
    QThread* targetThread)
{
    QnRequestParamList params;
    params.insert("source", source);
    params.insert("caption", caption);
    params.insert("description", description);
    if (toggleState != nx::vms::api::EventState::undefined)
        params.insert("state", QnLexical::serialized(toggleState));
    params.insert("metadata", QString::fromUtf8(QJson::serialized(metadata)));
    return executePost("/api/createEvent", params, callback, targetThread);
}

QnMediaServerResourcePtr ServerConnection::getServerWithInternetAccess() const
{
    QnMediaServerResourcePtr server =
        commonModule()->resourcePool()->getResourceById<QnMediaServerResource>(commonModule()->remoteGUID());
    if (!server)
        return QnMediaServerResourcePtr(); //< something wrong. No current server available

    if (server->getServerFlags().testFlag(vms::api::SF_HasPublicIP))
        return server;

    // Current server doesn't have internet access. Try to find another one
    for (const auto server: commonModule()->resourcePool()->getAllServers(Qn::Online))
    {
        if (server->getServerFlags().testFlag(vms::api::SF_HasPublicIP))
            return server;
    }
    return QnMediaServerResourcePtr(); //< no internet access found
}

void ServerConnection::trace(rest::Handle handle, const QString& message) const
{
    ::trace(m_serverId.toString(), handle, message);
}

Handle ServerConnection::getStatisticsSettingsAsync(
    Result<QByteArray>::type callback,
    QThread* targetThread)
{
    QnEmptyRequestData emptyRequest;
    emptyRequest.format = Qn::SerializationFormat::UbjsonFormat;
    static const auto path = lit("/ec2/statistics/settings");

    QnMediaServerResourcePtr server = getServerWithInternetAccess();
    if (!server)
        return Handle(); //< can't process request now. No internet access

    nx::network::http::ClientPool::Request request = prepareRequest(
        nx::network::http::Method::get, prepareUrl(path, emptyRequest.toParams()));
    nx::network::http::HttpHeader header(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    nx::network::http::insertOrReplaceHeader(&request.headers, header);
    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    NX_VERBOSE(m_logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::sendStatisticsAsync(
    const QnSendStatisticsRequestData& statisticsData,
    PostCallback callback,
    QThread* targetThread)
{
    static const auto path = lit("/ec2/statistics/send");

    auto server = getServerWithInternetAccess();
    if (!server)
        return Handle(); //< can't process request now. No internet access

    const nx::network::http::BufferType data = QJson::serialized(statisticsData.metricsList);
    if (data.isEmpty())
        return Handle();

    nx::network::http::ClientPool::Request request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl(path, statisticsData.toParams()),
        nx::network::http::header::ContentType::kJson,
        data);
    nx::network::http::HttpHeader header(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    nx::network::http::insertOrReplaceHeader(&request.headers, header);
    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    NX_VERBOSE(m_logTag, "<%1> %2", handle, request.url);
    return handle;
}

Handle ServerConnection::getModuleInformation(
    Result<RestResultWithData<nx::vms::api::ModuleInformation>>::type callback,
    QThread* targetThread)
{
    QnRequestParamList params;
    return executeGet("/api/moduleInformation", params, callback, targetThread);
}

Handle ServerConnection::getModuleInformationAll(
    Result<RestResultWithData<QList<nx::vms::api::ModuleInformation>>>::type callback,
    QThread* targetThread)
{
    QnRequestParamList params;
    params.insert("allModules", lit("true"));
    return executeGet("/api/moduleInformation", params, callback, targetThread);
}

Handle ServerConnection::getMediaServers(
    Result<nx::vms::api::MediaServerDataList>::type callback,
    QThread* targetThread)
{
    QnRequestParamList params;
    return executeGet("/ec2/getMediaServers", params, callback, targetThread);
}

Handle ServerConnection::detachSystemFromCloud(
    const QString& currentAdminPassword,
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

    CurrentPasswordData currentPasswordData;
    currentPasswordData.currentPassword = currentAdminPassword;

    return executePost(
        lit("/api/detachFromCloud"),
        QnRequestParamList(),
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        QJson::serialized(DetachFromCloudData(data, currentPasswordData)),
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
    return executePost(
        "/api/startLiteClient", {{"startCamerasMode", "true"}},
        callback, targetThread);
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
    qint64 size,
    const QByteArray& md5,
    const QUrl& url,
    const QString& peerPolicy,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        lit("/api/downloads/%1").arg(fileName),
        QnRequestParamList{
            {lit("size"), QString::number(size)},
            {lit("md5"), QString::fromUtf8(md5)},
            {lit("url"), url.toString()},
            {lit("peerPolicy"), peerPolicy}},
        callback,
        targetThread);
}

Handle ServerConnection::addCamera(
    const QnManualResourceSearchList& cameras,
    const QString& userName,
    const QString& password,
    GetCallback callback,
    QThread* thread)
{
    QnRequestParamList parameters;
    for (int i = 0; i < cameras.size(); i++)
    {
        const auto camera = cameras.at(i);
        const auto number = QString::number(i);
        parameters.insert(lit("url") + number, camera.url);
        parameters.insert(lit("manufacturer") + number, camera.manufacturer);
        parameters.insert(lit("uniqueId") + number, camera.uniqueId);
    }
    parameters.insert("user", userName);
    parameters.insert("password", password);

    return executePost(lit("/api/manualCamera/add"), parameters, callback, thread);
}

Handle ServerConnection::searchCameraStart(
    const QString& cameraUrl,
    const QString& userName,
    const QString& password,
    std::optional<int> port,
    GetCallback callback,
    QThread* targetThread)
{
    auto parameters = QnRequestParamList{
        {"url", cameraUrl},
        {"user", userName},
        {"password", password}};
    if (port.has_value())
        parameters.insert("port", *port);

    return executePost("/api/manualCamera/search", parameters, callback, targetThread);
}

Handle ServerConnection::searchCameraRangeStart(
    const QString& startAddress,
    const QString& endAddress,
    const QString& userName,
    const QString& password,
    std::optional<int> port,
    GetCallback callback,
    QThread* targetThread)
{
    NX_ASSERT(!endAddress.isEmpty());
    auto parameters = QnRequestParamList{
        {"start_ip", startAddress},
        {"user", userName},
        {"password", password},
        {"end_ip", endAddress}};
    if (port.has_value())
        parameters.insert("port", *port);

    return executePost("/api/manualCamera/search", parameters, callback, targetThread);
}

Handle ServerConnection::searchCameraStatus(
    const QnUuid& processUuid,
    GetCallback callback,
    QThread* targetThread)
{
    return executeGet(lit("/api/manualCamera/status"),
        QnRequestParamList{{lit("uuid"), processUuid.toString()}},
            callback, targetThread);
}

Handle ServerConnection::searchCameraStop(
    const QnUuid& processUuid,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(lit("/api/manualCamera/stop"),
        QnRequestParamList{{lit("uuid"), processUuid.toString()}},
            callback, targetThread);
}

Handle ServerConnection::executeAnalyticsAction(
    const AnalyticsAction& action,
    Result<QnJsonRestResult>::type callback,
    QThread* targetThread)
{
    return executePost(
        lit("/api/executeAnalyticsAction"),
        QnRequestParamList(),
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        QJson::serialized(action),
        callback,
        targetThread);
}

Handle ServerConnection::executeEventAction(
    const nx::vms::api::EventActionData& action,
    Result<QnJsonRestResult>::type callback,
    QThread* targetThread)
{
    return executePost(
        lit("/api/executeEventAction"),
        QnRequestParamList(),
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        QJson::serialized(action),
        callback,
        targetThread);
}

Handle ServerConnection::addFileUpload(
    const QString& fileName,
    qint64 size,
    qint64 chunkSize,
    const QByteArray& md5,
    qint64 ttl,
    bool recreateIfExists,
    AddUploadCallback callback,
    QThread* targetThread)
{
    QnRequestParamList params
    {
        {"size", QString::number(size)},
        {"chunkSize", QString::number(chunkSize)},
        {"md5", QString::fromUtf8(md5)},
        {"ttl", QString::number(ttl)},
        {"upload", "true"},
        {"recreate", recreateIfExists ? "true" : "false"},
    };
    const auto& path = QStringLiteral("/api/downloads/%1").arg(fileName);
    return executePost(path, params, QByteArray(), QByteArray(), callback, targetThread);
}

Handle ServerConnection::removeFileDownload(
    const QString& fileName,
    bool deleteData,
    PostCallback callback,
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
            {lit("chunkSize"), QString::number(chunkSize)},
            {lit("fromInternet"), lit("true")}},
        callback,
        targetThread);
}

Handle ServerConnection::downloadFileChunkFromInternetUsingServer(
    const QnUuid& server,
    const QString& fileName,
    const nx::utils::Url& url,
    int chunkIndex,
    int chunkSize,
    Result<QByteArray>::type callback,
    QThread* targetThread)
{
    QnRequestParamList params{
        {lit("url"), url.toString()},
        {lit("chunkSize"), QString::number(chunkSize)},
        {lit("fromInternet"), lit("true")}};
    QString path = lit("/api/downloads/%1/chunks/%2").arg(fileName).arg(chunkIndex);
    nx::network::http::ClientPool::Request request = prepareRequest(
            nx::network::http::Method::get, prepareUrl(path, params));
    nx::network::http::HttpHeader header(Qn::SERVER_GUID_HEADER_NAME, server.toByteArray());
    nx::network::http::insertOrReplaceHeader(&request.headers, header);
    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    NX_VERBOSE(m_logTag, "<%1> %2", handle, request.url);

    return handle;
}

Handle ServerConnection::uploadFileChunk(
    const QString& fileName,
    int index,
    const QByteArray& data,
    PostCallback callback,
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
    nx::vms::api::EventState toggleState,
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
        || rule->eventType() >= nx::vms::api::EventType::userDefinedEvent)
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

    if (toggleState != nx::vms::api::EventState::undefined)
        params.insert(lit("state"), QnLexical::serialized(toggleState));

    params.insert(lit("inputPortId"), rule->eventParams().inputPortId);
    params.insert(lit("source"), rule->eventParams().resourceName);
    params.insert(lit("caption"), rule->eventParams().caption);
    params.insert(lit("description"), rule->eventParams().description);
    params.insert(lit("metadata"), QString::fromUtf8(
        QJson::serialized(rule->eventParams().metadata)));

    return executePost(lit("/api/createEvent"), params, callback, targetThread);
}

Handle ServerConnection::mergeSystemAsync(
    const nx::utils::Url& url, const QString& getKey, const QString& postKey,
    bool ownSettings, bool oneServer, bool ignoreIncompatible,
    GetCallback callback, QThread* targetThread)
{
    QnRequestParamList params =
    {
        {"url", url.toString()},
        {"getKey", getKey},
        {"postKey", postKey},
        {"takeRemoteSettings", QnLexical::serialized(!ownSettings)},
        {"oneServer", QnLexical::serialized(oneServer)},
        {"ignoreIncompatible", QnLexical::serialized(ignoreIncompatible)},
    };

    return executePost("/api/mergeSystems", std::move(params), callback, targetThread);
}

Handle ServerConnection::pingSystemAsync(
    const nx::utils::Url& url, const QString& getKey,
    Result<RestResultWithData<nx::vms::api::ModuleInformation>>::type callback,
    QThread* targetThread)
{
    QnRequestParamList params;
    params.insert("url", url.toString());
    params.insert("getKey", getKey);
    return executeGet("/api/pingSystem", params, callback, targetThread);
}

Handle ServerConnection::getNonceAsync(const nx::utils::Url& url,
    Result<RestResultWithData<QnGetNonceReply>>::type callback,
    QThread* targetThread)
{
    QnRequestParamList params;
    params.insert("url", url.toString());
    return executeGet("/api/getRemoteNonce", params, callback, targetThread);
}

Handle ServerConnection::addWearableCamera(
    const QString& name,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        lit("/api/wearableCamera/add"),
        QnRequestParamList{ { lit("name"), name } },
        QByteArray(),
        QByteArray(),
        callback,
        targetThread);
}

Handle ServerConnection::prepareWearableUploads(
    const QnNetworkResourcePtr& camera,
    const QnWearablePrepareData& data,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        lit("/api/wearableCamera/prepare"),
        QnRequestParamList{ { lit("cameraId"), camera->getId().toSimpleString() } },
        nx::network::http::header::ContentType::kJson,
        QJson::serialized(data),
        callback,
        targetThread);
}

Handle ServerConnection::wearableCameraStatus(
    const QnNetworkResourcePtr& camera,
    GetCallback callback,
    QThread* targetThread)
{
    return executeGet(
        lit("/api/wearableCamera/status"),
        QnRequestParamList{ { lit("cameraId"), camera->getId().toSimpleString() } },
        callback,
        targetThread);
}

Handle ServerConnection::lockWearableCamera(
    const QnNetworkResourcePtr& camera,
    const QnUserResourcePtr& user,
    qint64 ttl,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        lit("/api/wearableCamera/lock"),
        QnRequestParamList{
            { lit("cameraId"), camera->getId().toSimpleString() },
            { lit("userId"), user->getId().toSimpleString() },
            { lit("ttl"), QString::number(ttl) } },
        callback,
        targetThread);
}

Handle ServerConnection::extendWearableCameraLock(
    const QnNetworkResourcePtr& camera,
    const QnUserResourcePtr& user,
    const QnUuid& token,
    qint64 ttl,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        lit("/api/wearableCamera/extend"),
        QnRequestParamList{
            { lit("cameraId"), camera->getId().toSimpleString() },
            { lit("token"), token.toSimpleString() },
            { lit("userId"), user->getId().toSimpleString() },
            { lit("ttl"), QString::number(ttl) } },
        callback,
        targetThread);
}

Handle ServerConnection::releaseWearableCameraLock(
    const QnNetworkResourcePtr& camera,
    const QnUuid& token,
    GetCallback callback,
    QThread* targetThread)
{
    return executePost(
        lit("/api/wearableCamera/release"),
        QnRequestParamList{
            { lit("cameraId"), camera->getId().toSimpleString() },
            { lit("token"), token.toSimpleString() } },
        callback,
        targetThread);
}

Handle ServerConnection::consumeWearableCameraFile(
    const QnNetworkResourcePtr& camera,
    const QnUuid& token,
    const QString& uploadId,
    qint64 startTimeMs,
    PostCallback callback,
    QThread* targetThread)
{
    return executePost(
        lit("/api/wearableCamera/consume"),
        QnRequestParamList{
            { lit("cameraId"), camera->getId().toSimpleString() },
            { lit("token"), token.toSimpleString() },
            { lit("uploadId"), uploadId },
            { lit("startTime"), QString::number(startTimeMs) } },
        callback,
        targetThread);
}

Handle ServerConnection::getStatistics(
    ServerConnection::GetCallback callback, QThread* targetThread)
{
    return executeGet("/api/statistics", {}, callback, targetThread);
}

Handle ServerConnection::getEvents(QnEventLogRequestData request,
    Result<EventLogData>::type callback,
    QThread* targetThread)
{
    request.format = Qn::SerializationFormat::UbjsonFormat;
    return executeGet(lit("/api/getEvents"), request.toParams(), callback, targetThread);
}

Handle ServerConnection::getEvents(const QnEventLogMultiserverRequestData& request,
    Result<EventLogData>::type callback,
    QThread* targetThread)
{
    return executeGet(lit("/ec2/getEvents"), request.toParams(), callback, targetThread);
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
    nx::network::http::ClientPool::Request request = prepareRequest(
        nx::network::http::Method::post,
        prepareUrl(lit("/api/changeCameraPassword"), params.toParams()));
    request.messageBody = QJson::serialized(std::move(data));
    nx::network::http::insertOrReplaceHeader(&request.headers,
        nx::network::http::HttpHeader(Qn::SERVER_GUID_HEADER_NAME, camera->getParentId().toByteArray()));

    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    NX_VERBOSE(m_logTag, "<%1> %2", handle, request.url);
    return handle;
}

int ServerConnection::checkCameraList(const QnNetworkResourceList& cameras,
    Result<QnCameraListReply>::type callback,
    QThread* targetThread)
{
    QnCameraListReply camList;
    for (const QnResourcePtr& c: cameras)
        camList.uniqueIdList << c->getUniqueId();

    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);

    return executePost("/api/checkDiscovery", QnRequestParamList(),
        contentType, QJson::serialized(camList), callback,  targetThread);
}


Handle ServerConnection::lookupObjectTracks(
    const nx::analytics::db::Filter& request,
    bool isLocal,
    Result<nx::analytics::db::LookupResult>::type callback,
    QThread* targetThread)
{
    QnRequestParamList queryParams;
    nx::analytics::db::serializeToParams(request, &queryParams);
    queryParams.insert(lit("isLocal"), isLocal? lit("true") : lit("false"));

    return executeGet(
        lit("/ec2/analyticsLookupObjectTracks"),
        queryParams,
        callback,
        targetThread);
}

Handle ServerConnection::updateActionStart(
    const nx::update::Information& info,
    std::function<void (Handle, bool)>&& callback,
    QThread* targetThread)
{
    auto internalCallback =
        [callback = std::move(callback)](
            bool success, rest::Handle handle, EmptyResponseType /*response*/)
        {
            if (callback)
                callback(success, handle);
        };
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
    auto request = QJson::serialized(info);
    return executePost<EmptyResponseType>("/ec2/startUpdate", QnRequestParamList(), contentType,
        request, internalCallback, targetThread);
}

Handle ServerConnection::getUpdateInfo(
    Result<UpdateInformationData>::type&& callback, QThread* targetThread)
{
    QnRequestParamList params;
    return executeGet("/ec2/updateInformation", params, callback, targetThread);
}

Handle ServerConnection::getUpdateInfo(
    const QString& version, Result<UpdateInformationData>::type&& callback, QThread* targetThread)
{
    QnRequestParamList params{{"version", version}};
    return executeGet("/ec2/updateInformation", params, callback, targetThread);
}

Handle ServerConnection::checkForUpdates(const QString& changeset,
    Result<UpdateInformationData>::type&& callback,
    QThread* targetThread)
{
    QnRequestParamList params {{"version", changeset}};
    return executeGet("/ec2/updateInformation", params, callback, targetThread);
}

Handle ServerConnection::getInstalledUpdateInfo(Result<UpdateInformationData>::type&& callback,
    QThread* targetThread)
{
    QnRequestParamList params {{"version", "installed"}};
    return executeGet("/ec2/updateInformation", params, callback, targetThread);
}

Handle ServerConnection::updateActionStop(
    std::function<void (bool, Handle)>&& callback, QThread* targetThread)
{
    auto internalCallback =
        [callback = std::move(callback)](
            bool success, rest::Handle handle, EmptyResponseType /*response*/)
        {
            if (callback)
                callback(success, handle);
        };
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
    return executePost<EmptyResponseType>("/ec2/cancelUpdate",
        QnRequestParamList(),
        contentType, QByteArray(),
        internalCallback, targetThread);
}

Handle ServerConnection::updateActionFinish(bool skipActivePeers,
    std::function<void (bool, Handle, const QnRestResult& result)>&& callback, QThread* targetThread)
{
    auto internalCallback =
        [callback=std::move(callback)](
            bool success, rest::Handle handle, const QnRestResult& result)
        {
            if (callback)
                callback(success, handle, result);
        };

    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);

    QnRequestParamList params;
    if (skipActivePeers)
        params.insert("ignorePendingPeers", "true");

    return executePost<QnRestResult>("/ec2/finishUpdate",
        params,
        contentType, QByteArray(),
        internalCallback, targetThread);
}

Handle ServerConnection::updateActionInstall(const QSet<QnUuid>& participants,
    std::function<void (bool, Handle, const QnRestResult& result)>&& callback,
    QThread* targetThread)
{
    auto internalCallback =
        [callback=std::move(callback)](
            bool success, rest::Handle handle, const QnRestResult& result)
        {
            if (callback)
                callback(success, handle, result);
        };
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
    QString peerList;
    for (const auto& peer: participants)
    {
        if (!peerList.isEmpty())
            peerList += ",";
        peerList += peer.toString();
    }
    return executePost<QnRestResult>("/api/installUpdate",
        QnRequestParamList{{ lit("peers"), peerList }},
        contentType, QByteArray(), internalCallback, targetThread);
}

Handle ServerConnection::retryUpdate(
    Result<UpdateStatusAllData>::type callback, QThread* targetThread)
{
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
    return executePost<UpdateStatusAllData>("/ec2/retryUpdate",
        QnRequestParamList(), contentType, QByteArray(), callback, targetThread);
}

Handle ServerConnection::getUpdateStatus(
    Result<UpdateStatusAllData>::type callback, QThread* targetThread)
{
    QnRequestParamList params;
    return executeGet("/ec2/updateStatus", params, callback, targetThread);
}

//--------------------------------------------------------------------------------------------------

Handle ServerConnection::getEngineAnalyticsSettings(
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    Result<nx::vms::api::analytics::SettingsResponse>::type&& callback,
    QThread* targetThread)
{
    return executeGet(
        "/ec2/analyticsEngineSettings",
        QnRequestParamList{
            {"analyticsEngineId", engine->getId().toString()}
        },
        Result<QnJsonRestResult>::type(
            [callback = std::move(callback)](
                bool success, Handle requestId, const QnJsonRestResult& result)
            {
                callback(
                    success,
                    requestId,
                    result.deserialized<nx::vms::api::analytics::SettingsResponse>());
            }),
        targetThread);
}

Handle ServerConnection::setEngineAnalyticsSettings(
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QJsonObject& settings,
    Result<nx::vms::api::analytics::SettingsResponse>::type&& callback,
    QThread* targetThread)
{
    return executePost<QnJsonRestResult>(
        QString("/ec2/analyticsEngineSettings"),
        QnRequestParamList{
            {"analyticsEngineId", engine->getId().toString()}
        },
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        QJson::serialized(settings),
        [callback = std::move(callback)](
            bool success, Handle requestId, const QnJsonRestResult& result)
        {
            callback(
                success,
                requestId,
                result.deserialized<nx::vms::api::analytics::SettingsResponse>());
        },
        targetThread);
}

Handle ServerConnection::getDeviceAnalyticsSettings(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    Result<nx::vms::api::analytics::SettingsResponse>::type&& callback,
    QThread* targetThread)
{
    return executeGet(
        "/ec2/deviceAnalyticsSettings",
        QnRequestParamList{
            {"deviceId", device->getId().toString()},
            {"analyticsEngineId", engine->getId().toString()},
        },
        Result<QnJsonRestResult>::type(
            [callback = std::move(callback)](
                bool success, Handle requestId, const QnJsonRestResult& result)
            {
                callback(
                    success,
                    requestId,
                    result.deserialized<nx::vms::api::analytics::SettingsResponse>());
            }),
        targetThread);
}

Handle ServerConnection::setDeviceAnalyticsSettings(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QJsonObject& settings,
    Result<nx::vms::api::analytics::SettingsResponse>::type&& callback,
    QThread* targetThread)
{
    return executePost<QnJsonRestResult>(
        QString("/ec2/deviceAnalyticsSettings"),
        QnRequestParamList{
            {"deviceId", device->getId().toString()},
            {"analyticsEngineId", engine->getId().toString()},
        },
        Qn::serializationFormatToHttpContentType(Qn::JsonFormat),
        QJson::serialized(settings),
        [callback = std::move(callback)](
            bool success, Handle requestId, const QnJsonRestResult& result)
        {
            callback(
                success,
                requestId,
                result.deserialized<nx::vms::api::analytics::SettingsResponse>());
        },
        targetThread);
}

Handle ServerConnection::postJsonResult(
    const QString& action,
    const QnRequestParamList& params,
    const nx::Buffer& body,
    rest::JsonResultCallback&& callback,
    QThread* targetThread,
    std::chrono::milliseconds timeout)
{
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
    return executePost<QnJsonRestResult>(action,
        params,
        contentType, body,
        callback,
        targetThread,
        timeout);
}

Handle ServerConnection::postEmptyResult(
    const QString& action,
    const QnRequestParamList& params,
    const nx::Buffer& body,
    PostCallback&& callback,
    QThread* targetThread)
{
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::UbjsonFormat);
    return executePost<EmptyResponseType>(action,
        params,
        contentType, body,
        callback,
        targetThread);
}

Handle ServerConnection::getUbJsonResult(
    const QString& path,
    const QnRequestParamList& params,
    std::function<void(bool, Handle, QnUbjsonRestResult response)>&& callback,
    QThread* targetThread)
{
    return executeGet(path, params, callback, targetThread);
}

Handle ServerConnection::getJsonResult(
    const QString& path,
    const QnRequestParamList& params,
    rest::JsonResultCallback&& callback,
    QThread* targetThread)
{
    return executeGet(path, params, callback, targetThread);
}

Handle ServerConnection::getRawResult(
    const QString& path,
    const QnRequestParamList& params,
    std::function<void(bool, Handle, QByteArray, nx::network::http::HttpHeaders)> callback,
    QThread* targetThread)
{
    return executeGet(path, params, callback, targetThread);
}

Handle ServerConnection::postUbJsonResult(
    const QString& action,
    const QnRequestParamList& params,
    const nx::Buffer& body,
    std::function<void(bool, Handle, const QnUbjsonRestResult& response)>&& callback,
    QThread* targetThread)
{
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::UbjsonFormat);
    return executePost<QnUbjsonRestResult>(action,
        params,
        contentType, body,
        callback,
        targetThread);
}

Handle ServerConnection::getPluginInformation(GetCallback callback, QThread* targetThread)
{
    return executeGet("/api/pluginInfo", {}, callback, targetThread);
}

Handle ServerConnection::testEmailSettings(
      const QnEmailSettings& settings,
      Result<QnTestEmailSettingsReply>::type&& callback,
      QThread* targetThread)
{
    nx::vms::api::EmailSettingsData data;
    ec2::fromResourceToApi(settings, data);
    const auto contentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);
    auto body = QJson::serialized(data);
    return executePost("/api/testEmailSettings", {}, contentType, body, callback, targetThread);
}

Handle ServerConnection::getAuditLog(
    qint64 startTimeMs,
    qint64 endTimeMs,
    Result<QnAuditRecordList>::type&& callback,
    QThread* targetThread)
{
    QnRequestParamList params;
    params.insert("from", startTimeMs * 1000ll);
    params.insert("to", endTimeMs * 1000ll);
    params.insert("format", "ubjson");

    return executeGet("/api/auditLog", params, callback, targetThread);
}

Handle ServerConnection::getSystemId(
    Result<QString>::type&& callback,
    QThread* targetThread)
{
    auto internalCallback =
        [callback=std::move(callback)](
            bool success, Handle requestId, QByteArray result,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            callback(success, requestId, QString::fromUtf8(result));
        };
    return executeGet("/api/getSystemId", {}, internalCallback, targetThread);
}

Handle ServerConnection::doCameraDiagnosticsStep(
    const QnUuid& cameraId, CameraDiagnostics::Step::Value previousStep,
    Result<RestResultWithData<QnCameraDiagnosticsReply>>::type&& callback,
    QThread* targetThread)
{
    QnRequestParamList params;
    params.insert("cameraId", cameraId);
    params.insert("type", CameraDiagnostics::Step::toString(previousStep));

    return executeGet("/api/doCameraDiagnosticsStep", params, callback, targetThread);
}

template<class ResultType>
std::function<void(ServerConnection::ContextPtr context)> makeCallbackWithErrorMessage(
    std::function<void(
        bool success,
        Handle requestId,
        const ResultType& result,
        const QString& message)> callback, QnUuid serverId)
{
    return [callback, serverId](ServerConnection::ContextPtr context)
        {
            bool success = false;
            const auto format = Qn::serializationFormatFromHttpContentType(context->response.contentType);
            bool goodFormat = format == Qn::JsonFormat || format == Qn::UbjsonFormat;

            std::shared_ptr<ResultType> result;

            QString errorString;
            if (context->systemError != SystemError::noError
                || context->getStatusCode() != nx::network::http::StatusCode::ok)
            {
                success = false;
            }

            if (success && goodFormat)
            {
                QnJsonRestResult restResult;
                if (!QJson::deserialize(context->response.messageBody, &restResult))
                    success = false;

                //else
                //    QJson::deserialize(restResult.reply, &result);
            }

            auto internal_callback =
                [callback, success, id = context->handle, result, errorString]()
                {
                    if (callback)
                        callback(success, id, result ? std::move(*result) : ResultType(), errorString);
                };

            invoke(context, internal_callback, success, serverId.toString());
        };
}

Handle ServerConnection::testLdapSettingsAsync(const QnLdapSettings& settings,
    LdapSettingsCallback&& callback,
    QThread* targetThread)
{
    QnRequestParamList params;
    params.insert(nx::network::http::header::kContentType, "application/json");
    std::chrono::seconds timeout(settings.searchTimeoutS);

    auto callback_wrapper = [callback](bool success, int handle, QnJsonRestResult data)
        {
            auto response = data.deserialized<QnLdapUsers>(&success);
            callback(success, handle, response, data.errorString);
        };

    return postJsonResult("/api/testLdapSettings", {},
        QJson::serialized(settings), callback_wrapper, targetThread, timeout);
}

Handle ServerConnection::recordedTimePeriods(
    const QnChunksRequestData& request,
    Result<MultiServerPeriodDataList>::type&& callback,
    QThread* targetThread)
{
    QnChunksRequestData fixedFormatRequest(request);
    fixedFormatRequest.format = Qn::CompressedPeriodsFormat;
    auto internalCallback =
        [callback=std::move(callback)](
            bool success, Handle requestId, QByteArray result,
            const nx::network::http::HttpHeaders& /*headers*/)
        {
            if (success)
            {
                bool goodData = false;
                auto chunks = QnCompressedTime::deserialized<MultiServerPeriodDataList>(
                    result, {}, &goodData);
                callback(goodData, requestId, chunks);
            }
            callback(false, requestId, {});
        };
    return executeGet("/ec2/recordedTimePeriods", fixedFormatRequest.toParams(), internalCallback,
        targetThread);
}

Handle ServerConnection::debug(
    const QString& action, const QString& value, PostCallback callback, QThread* targetThread)
{
    return executeGet("/api/debug", {{action, value}}, callback, targetThread);
}

// --------------------------- private implementation -------------------------------------

QUrl ServerConnection::prepareUrl(const QString& path, const QnRequestParamList& params) const
{
    QUrl result;
    result.setPath(path);
    result.setQuery(params.toUrlQuery());
    return result;
}

template<typename CallbackType>
Handle ServerConnection::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    CallbackType callback,
    QThread* targetThread)
{
    auto request = this->prepareRequest(nx::network::http::Method::get, prepareUrl(path, params));
    auto handle = request.isValid()
        ? this->executeRequest(request, callback, targetThread)
        : Handle();

    NX_VERBOSE(m_logTag, "<%1> %2", handle, request.url);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executePost(
    const QString& path,
    const QnRequestParamList& params,
    Callback<ResultType> callback,
    QThread* targetThread,
    std::chrono::milliseconds timeout)
{
    return executePost(
        path, {},
        nx::network::http::header::ContentType::kJson, QJson::serialized(params.toJson()),
        std::move(callback), targetThread, timeout);
}

template <typename ResultType>
Handle ServerConnection::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const nx::network::http::StringType& contentType,
    const nx::network::http::StringType& messageBody,
    Callback<ResultType> callback,
    QThread* targetThread,
    std::chrono::milliseconds timeout)
{
    auto request = this->prepareRequest(
        nx::network::http::Method::post, prepareUrl(path, params), contentType, messageBody);
    auto handle = request.isValid()
        ? this->executeRequest(request, callback, targetThread, timeout)
        : Handle();

    NX_VERBOSE(m_logTag, "<%1> %2", handle, request.url);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executePut(
    const QString& path,
    const QnRequestParamList& params,
    const nx::network::http::StringType& contentType,
    const nx::network::http::StringType& messageBody,
    Callback<ResultType> callback,
    QThread* targetThread)
{
    auto request = prepareRequest(
        nx::network::http::Method::put, prepareUrl(path, params), contentType, messageBody);
    auto handle = request.isValid()
        ? executeRequest(request, callback, targetThread)
        : Handle();

    NX_VERBOSE(m_logTag, "<%1> %2", handle, request.url);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executeDelete(
    const QString& path,
    const QnRequestParamList& params,
    Callback<ResultType> callback,
    QThread* targetThread)
{
    auto request = prepareRequest(nx::network::http::Method::delete_, prepareUrl(path, params));
    auto handle = request.isValid()
        ? executeRequest(request, callback, targetThread)
        : Handle();

    NX_VERBOSE(m_logTag, "<%1> %2", handle, request.url);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executeRequest(
    const nx::network::http::ClientPool::Request& request,
    Callback<ResultType> callback,
    QThread* targetThread,
    std::chrono::milliseconds timeout)
{
    if (callback)
    {
        const QString serverId = m_serverId.toString();
        return sendRequest(request,
            [this, callback, serverId](ContextPtr context)
            {
                NX_VERBOSE(m_logTag, "<%1> Got serialized reply. OS error: %2, HTTP status: %3",
                    context->handle, context->systemError, context->getStatusCode());
                bool success = false;
                const auto format = Qn::serializationFormatFromHttpContentType(context->response.contentType);
                bool goodFormat = format == Qn::JsonFormat || format == Qn::UbjsonFormat;

                auto result = std::make_shared<ResultType>(goodFormat
                    ? parseMessageBody<ResultType>(format, context->response.messageBody, &success)
                    : ResultType());

                if (!success)
                    NX_VERBOSE(m_logTag, "<%1> Could not parse message body.", context->handle);

                if (context->systemError != SystemError::noError
                    || context->getStatusCode() != nx::network::http::StatusCode::ok)
                {
                    success = false;
                }

                const auto id = context->handle;

                auto internal_callback = [callback, success, id, result]()
                    {
                        if (callback)
                            callback(success, id, std::move(*result));
                    };

                invoke(context, internal_callback, success, serverId);
            }, targetThread, timeout);
    }

    return sendRequest(request, {}, targetThread);
}

// This is a specialization for request with QByteArray in response. Its callback is a bit different
// from regular Result<SomeType>::type. Result<QByteArray>::type has 4 arguments:
// `(bool success, Handle requestId, QByteArray result, nx::network::http::HttpHeaders& headers)`
Handle ServerConnection::executeRequest(
    const nx::network::http::ClientPool::Request& request,
    Result<QByteArray>::type callback,
    QThread* targetThread,
    std::chrono::milliseconds timeout)
{
    if (callback)
    {
        const QString serverId = m_serverId.toString();
        QPointer<QThread> targetThreadGuard(targetThread);
        return sendRequest(request,
            [this, callback,  targetThreadGuard, serverId](ContextPtr context)
            {
                const auto statusCode = context->getStatusCode();
                const auto osErrorCode = context->systemError;
                const auto id = context->handle;

                NX_VERBOSE(m_logTag,
                    "<%1> Got %2 byte(s) reply of content type %3. OS error: %4, HTTP status: %5",
                    id, context->response.messageBody.size(), QString::fromLatin1(context->response.contentType), osErrorCode, statusCode);

                bool success = (osErrorCode == SystemError::noError
                    && statusCode >= nx::network::http::StatusCode::ok
                    && statusCode <= nx::network::http::StatusCode::partialContent);

                auto internal_callback = [callback, success, id, context]()
                    {
                        callback(success, id, context->response.messageBody, context->response.headers);
                    };

                invoke(context, internal_callback, success, serverId);
            }, targetThread, timeout);
    }

    return sendRequest(request, {}, targetThread, timeout);
}

Handle ServerConnection::executeRequest(
    const nx::network::http::ClientPool::Request& request,
    Callback<EmptyResponseType> callback,
    QThread* targetThread,
    std::chrono::milliseconds timeout)
{
    if (callback)
    {
        const QString serverId = m_serverId.toString();
        return sendRequest(request,
            [this, callback, serverId](ContextPtr context)
            {
                const auto statusCode = context->getStatusCode();
                const auto osErrorCode = context->systemError;
                const auto id = context->handle;
                bool success = (osErrorCode == SystemError::noError
                    && statusCode >= nx::network::http::StatusCode::ok
                    && statusCode <= nx::network::http::StatusCode::partialContent);

                NX_VERBOSE(m_logTag, "<%1> Got reply. OS error: %2, HTTP status: %3",
                    id, osErrorCode, statusCode);

                auto internal_callback = [callback, success, id]()
                    {
                        callback(success, id, EmptyResponseType());
                    };

                invoke(context, internal_callback, success, serverId);
            }, targetThread, timeout);
    }

    return sendRequest(request, {}, targetThread, timeout);
}

void ServerConnection::cancelRequest(const Handle& requestId)
{
    NX_VERBOSE(m_logTag, "<%1> Cancelling request...", requestId);
    httpClientPool()->terminate(requestId);
    QnMutexLocker lock(&m_mutex);
    m_runningRequests.remove(requestId);
}

QnUuid ServerConnection::getServerId() const
{
    return m_serverId;
}

std::pair<QString, QString> getRequestCredentials(
    std::shared_ptr<ec2::AbstractECConnection> connection,
    const QnMediaServerResourcePtr& targetServer)
{
    using namespace nx::vms::api;
    const auto localPeerType = qnStaticCommon->localPeerType();
    if (PeerData::isClient(localPeerType))
    {
        const auto ecUrl = connection->connectionInfo().ecUrl;
        return std::make_pair(ecUrl.userName(), ecUrl.password());
    }

    const auto targetServerAuthCredentials =
        std::make_pair(targetServer->getId().toString(), targetServer->getAuthKey());

    if (PeerData::isServer(localPeerType))
        return targetServerAuthCredentials;

    NX_ASSERT(false, "Not an expected peer type");
    return targetServerAuthCredentials;
}

bool setupAuth(
    QnCommonModule* commonModule, QnUuid serverId,
    nx::network::http::ClientPool::Request& request,
    nx::network::http::Method::ValueType method,
    const QUrl& url)
{
    auto resPool = commonModule->resourcePool();
    const auto server = resPool->getResourceById<QnMediaServerResource>(serverId);
    if (!server)
        return false;
    const auto connection = commonModule->ec2Connection();
    if (!connection)
        return false;

    request.method = method;
    request.url = server->getApiUrl();
    request.url.setPath(url.path());
    request.url.setQuery(url.query());

    const auto [user, password] =  getRequestCredentials(connection, server);

    auto videoWallGuid = commonModule->videowallGuid();
    if (!videoWallGuid.isNull())
        request.headers.emplace(Qn::VIDEOWALL_GUID_HEADER_NAME, videoWallGuid.toByteArray());
    request.headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    request.headers.emplace(
        Qn::EC2_RUNTIME_GUID_HEADER_NAME, commonModule->runningInstanceGUID().toByteArray());
    request.headers.emplace(Qn::CUSTOM_USERNAME_HEADER_NAME, user.toLower().toUtf8());

    const auto& router = commonModule->router();
    QnRoute route = router->routeTo(server->getId());
    if (route.reverseConnect)
    {
        // no route exists. proxy via nearest server
        auto nearestServer = commonModule->currentServer();
        if (!nearestServer)
            return false;
        nx::utils::Url nearestUrl(server->getApiUrl());
        if (nearestServer->getId() == commonModule->moduleGUID())
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
    return true;
}

void setupAuthDirect(
    nx::utils::Url directUrl,
    nx::network::http::ClientPool::Request& request,
    nx::network::http::Method::ValueType method,
    const QUrl& url)
{
    // No actual authentication is required.
    request.method = method;
    request.url = directUrl;
    request.url.setPath(url.path());
    request.url.setQuery(url.query());
}

nx::network::http::ClientPool::Request ServerConnection::prepareRequest(
    nx::network::http::Method::ValueType method,
    const QUrl& url,
    const nx::network::http::StringType& contentType,
    const nx::network::http::StringType& messageBody)
{
    nx::network::http::ClientPool::Request request;

    if (!m_directUrl.isEmpty())
        setupAuthDirect(m_directUrl, request, method, url);
    else if (!setupAuth(commonModule(), m_serverId, request, method, url))
        return nx::network::http::ClientPool::Request();
    request.contentType = contentType;
    request.messageBody = messageBody;

    return request;
}

ServerConnection::ContextPtr ServerConnection::prepareContext(
    nx::network::http::Method::ValueType method,
    const QUrl& url) const
{
    ContextPtr context(new network::http::ClientPool::Context());
    if (!m_directUrl.isEmpty())
    {
        setupAuthDirect(m_directUrl, context->request, method, url);
    }
    else if (!setupAuth(commonModule(), m_serverId, context->request, method, url))
    {
        // We heed a better way to handle such error.
        return nullptr;
    }
    return context;
}

Handle ServerConnection::sendRequest(
    const nx::network::http::ClientPool::Request& request,
    std::function<void (ContextPtr)> callback,
    QThread* thread,
    std::chrono::milliseconds timeout)
{
    ContextPtr context(new nx::network::http::ClientPool::Context());
    context->request = request;
    context->completionFunc = callback;
    context->timeout = timeout;
    context->setTargetThread(thread);
    // Request can be complete just inside `sendRequest`, so requestId is already invalid.
    QnMutexLocker lock(&m_mutex);
    Handle requestId = httpClientPool()->sendRequest(context);
    if (!requestId || context->isFinished())
        return 0;
    m_runningRequests.insert(requestId);
    return requestId;
}

Handle ServerConnection::sendRequest(const ContextPtr& context)
{
    Handle requestId = httpClientPool()->sendRequest(context);
    // Request can be complete just inside `sendRequest`, so requestId is already invalid.
    QnMutexLocker lock(&m_mutex);
    if (!requestId || context->isFinished())
        return 0;
    m_runningRequests.insert(requestId);
    return requestId;
}

} // namespace rest
