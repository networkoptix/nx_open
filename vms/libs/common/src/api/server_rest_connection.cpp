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
#include <api/helpers/event_log_multiserver_request_data.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/router.h>
#include <utils/common/delayed.h>
#include <utils/common/synctime.h>

#include <nx/api/mediaserver/image_request.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/custom_headers.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/random.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data_fwd.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/rule.h>
#include <api/model/detach_from_cloud_data.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/api/data/peer_data.h>
#include <common/static_common_module.h>

using namespace nx;

namespace {

static const nx::network::http::StringType kJsonContentType = Qn::serializationFormatToHttpContentType(Qn::JsonFormat);

void trace(const QString& serverId, rest::Handle handle, const QString& message)
{
    static const nx::utils::log::Tag kTag(typeid(rest::ServerConnection));
    NX_VERBOSE(kTag) << lm("%1 <%2>: %3").args(serverId, handle, message);
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
    Qn::directConnect(
        httpClientPool(), &nx::network::http::ClientPool::done,
        this, &ServerConnection::onHttpClientDone);
}

ServerConnection::~ServerConnection()
{
    directDisconnectAll();
    for (const auto& handle : m_runningRequests.keys())
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
    nx::vms::api::EventState toggleState, GetCallback callback, QThread* targetThread)
{
    QnRequestParamList params;
    params.insert(lit("timestamp"), lit("%1").arg(qnSyncTime->currentMSecsSinceEpoch()));
    params.insert(lit("event_type"), QnLexical::serialized(nx::vms::api::EventType::softwareTriggerEvent));
    params.insert(lit("inputPortId"), triggerId);
    params.insert(lit("eventResourceId"), cameraId.toString());
    if (toggleState != nx::vms::api::EventState::undefined)
        params.insert(lit("state"), QnLexical::serialized(toggleState));
    return executeGet(lit("/api/createEvent"), params, callback, targetThread);
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
    return executeGet("/api/createEvent", params, callback, targetThread);
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
    trace(handle, path);
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
        kJsonContentType,
        data);
    nx::network::http::HttpHeader header(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    nx::network::http::insertOrReplaceHeader(&request.headers, header);
    auto handle = request.isValid() ? executeRequest(request, callback, targetThread) : Handle();
    trace(handle, path);
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
    params << QnRequestParam("allModules", lit("true"));
    return executeGet("/api/moduleInformation", params, callback, targetThread);
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
        QByteArray(),
        QByteArray(),
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
        parameters << QnRequestParam(lit("url") + number, camera.url);
        parameters << QnRequestParam(lit("manufacturer") + number, camera.manufacturer);
        parameters << QnRequestParam(lit("uniqueId") + number, camera.uniqueId);
    }
    parameters << QnRequestParam("user", userName);
    parameters << QnRequestParam("password", password);

    return executeGet(lit("/api/manualCamera/add"), parameters, callback, thread);
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
        parameters << QnRequestParam("port", *port);

    return executeGet("/api/manualCamera/search", parameters, callback, targetThread);
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
        parameters << QnRequestParam("port", *port);

    return executeGet("/api/manualCamera/search", parameters, callback, targetThread);
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
    return executeGet(lit("/api/manualCamera/stop"),
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

    return executeGet(lit("/api/createEvent"), params, callback, targetThread);
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

    return executeGet("/api/mergeSystems", std::move(params), callback, targetThread);
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
        kJsonContentType,
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
        QByteArray(),
        QByteArray(),
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
        QByteArray(),
        QByteArray(),
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
        QByteArray(),
        QByteArray(),
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
        QByteArray(),
        QByteArray(),
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
    trace(handle, request.url.toString());
    return handle;
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

Handle ServerConnection::getPluginInformation(GetCallback callback, QThread* targetThread)
{
    return executeGet("/api/pluginInfo", {}, callback, targetThread);
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

template<typename T,
    typename std::enable_if<!std::is_base_of<RestResultWithDataBase, T>::value>::type* = nullptr>
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

    trace(handle, path);
    return handle;
}

template <typename ResultType>
Handle ServerConnection::executePost(
    const QString& path,
    const QnRequestParamList& params,
    const nx::network::http::StringType& contentType,
    const nx::network::http::StringType& messageBody,
    Callback<ResultType> callback,
    QThread* targetThread)
{
    auto request = this->prepareRequest(
        nx::network::http::Method::post, prepareUrl(path, params), contentType, messageBody);
    auto handle = request.isValid()
        ? this->executeRequest(request, callback, targetThread)
        : Handle();

    trace(handle, path);
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
    auto request = prepareRequest(nx::network::http::Method::delete_, prepareUrl(path, params));
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

    if (!callback)
        return;

    if (targetThread)
    {
         auto ptr = std::make_shared<ResultType>(std::move(result));
         executeLaterInThread([callback, success, id, ptr]() mutable
             {
                 callback(success, id, std::move(*ptr));
             },
             targetThread);
    }
    else
    {
        callback(success, id, std::move(result));
    }
}

void invoke(ServerConnection::Result<QByteArray>::type callback,
    QThread* targetThread,
    bool success,
    const Handle& id,
    QByteArray result,
    const nx::network::http::HttpHeaders& headers,
    const QString &serverId,
    const QElapsedTimer& elapsed
)
{
    if (success)
        trace(serverId, id, lit("Reply success for %1ms").arg(elapsed.elapsed()));
    else
        trace(serverId, id, lit("Reply failed for %1ms").arg(elapsed.elapsed()));

    if (!callback)
        return;

    if (targetThread)
    {
        auto ptr = std::make_shared<QByteArray>(result);
        executeLaterInThread(
            [callback, headers, success, id, ptr]() mutable
            {
                callback(success, id, std::move(*ptr), headers);
            },
            targetThread);
    }
    else
    {
        callback(success, id, result, headers);
    }
}

template <typename ResultType>
Handle ServerConnection::executeRequest(
    const nx::network::http::ClientPool::Request& request,
    Callback<ResultType> callback,
    QThread* targetThread)
{
    if (callback)
    {
        const QString serverId = m_serverId.toString();
        QElapsedTimer timer;
        timer.start();

        return sendRequest(request,
            [this, callback, targetThread, serverId, timer](
                Handle id,
                SystemError::ErrorCode osErrorCode,
                int statusCode,
                nx::network::http::StringType contentType,
                nx::network::http::BufferType msgBody,
                const nx::network::http::HttpHeaders& /*headers*/)
            {
                NX_VERBOSE(m_logTag, "<%1> Got serialized reply. OS error: %2, HTTP status: %3",
                    id, osErrorCode, statusCode);

                bool success = false;
                const auto format = Qn::serializationFormatFromHttpContentType(contentType);
                bool goodFormat = format == Qn::JsonFormat || format == Qn::UbjsonFormat;
                auto result = goodFormat ?
                    parseMessageBody<ResultType>(format, msgBody, &success) : ResultType();

                if (!success)
                    NX_VERBOSE(m_logTag, "<%1> Could not parse message body.", id);

                if (osErrorCode != SystemError::noError
                    || statusCode != nx::network::http::StatusCode::ok)
                {
                    success = false;
                }
                invoke(callback, targetThread, success, id, std::move(result), serverId, timer);
            });
    }

    return sendRequest(request);
}

Handle ServerConnection::executeRequest(
    const nx::network::http::ClientPool::Request& request,
    Result<QByteArray>::type callback,
    QThread* targetThread)
{
    if (callback)
    {
        const QString serverId = m_serverId.toString();
        QElapsedTimer timer;
        timer.start();

        QPointer<QThread> targetThreadGuard(targetThread);
        return sendRequest(request,
            [this, callback, targetThread, targetThreadGuard, serverId, timer](
                Handle id,
                SystemError::ErrorCode osErrorCode,
                int statusCode,
                nx::network::http::StringType contentType,
                nx::network::http::BufferType msgBody,
                const nx::network::http::HttpHeaders& headers)
            {
                NX_VERBOSE(m_logTag,
                    "<%1> Got %2 byte(s) reply of content type %3. OS error: %4, HTTP status: %5",
                    id, msgBody, QString::fromLatin1(contentType), osErrorCode, statusCode);

                bool success = (osErrorCode == SystemError::noError
                    && statusCode >= nx::network::http::StatusCode::ok
                    && statusCode <= nx::network::http::StatusCode::partialContent);

                if (targetThread && targetThreadGuard.isNull())
                    return;

                invoke(callback, targetThread, success, id, std::move(msgBody), headers, serverId,
                    timer);
            });
    }

    return sendRequest(request);
}

Handle ServerConnection::executeRequest(
    const nx::network::http::ClientPool::Request& request,
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
            [this, callback, targetThread, targetThreadGuard, serverId, timer](
                Handle id,
                SystemError::ErrorCode osErrorCode,
                int statusCode,
                nx::network::http::StringType /*contentType*/,
                nx::network::http::BufferType /*msgBody*/,
                const nx::network::http::HttpHeaders& /*headers*/)
            {
                NX_VERBOSE(m_logTag, "<%1> Got reply. OS error: %2, HTTP status: %3",
                    id, osErrorCode, statusCode);

                bool success = (osErrorCode == SystemError::noError
                    && statusCode >= nx::network::http::StatusCode::ok
                    && statusCode <= nx::network::http::StatusCode::partialContent);

                if (targetThread && targetThreadGuard.isNull())
                    return;

                invoke(callback, targetThread, success, id, EmptyResponseType(), serverId, timer);
            });
    }

    return sendRequest(request);
}

void ServerConnection::cancelRequest(const Handle& requestId)
{
    NX_VERBOSE(m_logTag, "<%1> Cancelling request...", requestId);
    httpClientPool()->terminate(requestId);
    QnMutexLocker lock(&m_mutex);
    m_runningRequests.remove(requestId);
}

nx::network::http::ClientPool::Request ServerConnection::prepareDirectRequest(
    nx::network::http::Method::ValueType method,
    const QUrl& url,
    const nx::network::http::StringType& contentType,
    const nx::network::http::StringType& messageBody)
{
    nx::network::http::ClientPool::Request request;
    request.method = method;
    request.url = m_directUrl;
    request.url.setPath(url.path());
    request.url.setQuery(url.query());
    request.contentType = contentType;
    request.messageBody = messageBody;

    return request;
}

QnUuid ServerConnection::getServerId() const
{
    return m_serverId;
}

std::pair<QString, QString> ServerConnection::getRequestCredentials(
    const QnMediaServerResourcePtr& targetServer) const
{
    using namespace nx::vms::api;
    const auto localPeerType = qnStaticCommon->localPeerType();
    if (PeerData::isClient(localPeerType))
    {
        const auto ecUrl = commonModule()->ec2Connection()->connectionInfo().ecUrl;
        return std::make_pair(ecUrl.userName(), ecUrl.password());
    }

    const auto targetServerAuthCredentials =
        std::make_pair(targetServer->getId().toString(), targetServer->getAuthKey());

    if (PeerData::isServer(localPeerType))
        return targetServerAuthCredentials;

    NX_ASSERT(false, "Not an expected peer type");
    return targetServerAuthCredentials;
}

nx::network::http::ClientPool::Request ServerConnection::prepareRequest(
    nx::network::http::Method::ValueType method,
    const QUrl& url,
    const nx::network::http::StringType& contentType,
    const nx::network::http::StringType& messageBody)
{
    if (!m_directUrl.isEmpty())
        return prepareDirectRequest(method, url, contentType, messageBody);
    auto resPool = commonModule()->resourcePool();
    const auto server = resPool->getResourceById<QnMediaServerResource>(m_serverId);
    if (!server)
        return nx::network::http::ClientPool::Request();
    const auto connection = commonModule()->ec2Connection();
    if (!connection)
        return nx::network::http::ClientPool::Request();

    nx::network::http::ClientPool::Request request;
    request.method = method;
    request.url = server->getApiUrl();
    request.url.setPath(url.path());
    request.url.setQuery(url.query());
    request.contentType = contentType;
    request.messageBody = messageBody;

    const auto [user, password] =  getRequestCredentials(server);

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
            return nx::network::http::ClientPool::Request();
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
    const nx::network::http::ClientPool::Request& request,
    HttpCompletionFunc callback)
{
    QnMutexLocker lock(&m_mutex);
    Handle requestId = httpClientPool()->sendRequest(request);
    m_runningRequests.insert(requestId, callback);
    return requestId;
}

void ServerConnection::onHttpClientDone(
    int requestId, nx::network::http::AsyncHttpClientPtr httpClient)
{
    QnMutexLocker lock(&m_mutex);
    auto itr = m_runningRequests.find(requestId);
    if (itr == m_runningRequests.end())
        return; //< Request cancelled.

    HttpCompletionFunc callback = itr.value();
    m_runningRequests.remove(requestId);

    SystemError::ErrorCode systemError = SystemError::noError;
    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::ok;
    nx::network::http::StringType contentType;
    nx::network::http::BufferType messageBody;

    if (httpClient->failed())
    {
        systemError = SystemError::connectionReset;
    }
    else
    {
        statusCode =
            (nx::network::http::StatusCode::Value) httpClient->response()->statusLine.statusCode;
        contentType = httpClient->contentType();
        messageBody = httpClient->fetchMessageBodyBuffer();
    }
    lock.unlock();
    if (callback)
    {
        const auto response = httpClient->response();
        const auto headers = response ? response->headers : nx::network::http::HttpHeaders();
        callback(requestId, systemError, statusCode, contentType, messageBody, headers);
    }
};

} // namespace rest
