#include "media_server_connection.h"

#include <QtCore/QSharedPointer>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkReply>

#include <nx/utils/uuid.h>

#include <api/helpers/chunks_request_data.h>

#include <core/resource/camera_advanced_param.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>

#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>
#include <core/ptz/ptz_data.h>

#include <nx_ec/data/api_conversion_functions.h>

#include <utils/common/ldap.h>
#include <utils/common/warnings.h>
#include <utils/common/request_param.h>

#include <nx/vms/api/data/email_settings_data.h>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/compressed_time_functions.h>
#include <nx/network/http/http_types.h>

#include <api/app_server_connection.h>

#include <recording/time_period_list.h>

#include "network_proxy_factory.h"
#include "session_manager.h"
#include "media_server_reply_processor.h"

#include "model/camera_list_reply.h"
#include "model/configure_reply.h"
#include "model/upload_update_reply.h"
#include <nx/network/http/custom_headers.h>
#include "model/recording_stats_reply.h"
#include <api/model/getnonce_reply.h>
#include "common/common_module.h"

#include <nx/utils/log/log.h>
#include <nx/utils/datetime.h>

using namespace nx;

namespace {

// TODO: Introduce constants for API methods registered in media_server_process.cpp.
QN_DEFINE_LEXICAL_ENUM(RequestObject,
    (StorageStatusObject, "storageStatus")
    (StorageSpaceObject, "storageSpace")
    (GetParamsObject, "getCameraParam")
    (SetParamsObject, "setCameraParam")
    (CameraAddObject, "manualCamera/add")
    (checkCamerasObject, "checkDiscovery")
    (CameraDiagnosticsObject, "doCameraDiagnosticsStep")
    (GetSystemIdObject, "getSystemId")
    (RebuildArchiveObject, "rebuildArchive")
    (BackupControlObject, "backupControl")
    (PingSystemObject, "pingSystem")
    (GetNonceObject, "getRemoteNonce")
    (RecordingStatsObject, "recStats")
    (AuditLogObject, "auditLog")
    (TestEmailSettingsObject, "testEmailSettings")
    (TestLdapSettingsObject, "testLdapSettings")
    (ec2RecordedTimePeriodsObject, "ec2/recordedTimePeriods")
);

void trace(const QString& serverId, int handle, int obj, const QString& message = QString())
{
    static const nx::utils::log::Tag kTag(typeid(QnMediaServerConnection));
    NX_VERBOSE(kTag) << lm("%1 <%2>: %3 %4").args(
        serverId, handle, message, QnLexical::serialized(static_cast<RequestObject>(obj)));
}

nx::network::http::HttpHeaders makeDefaultHeaders()
{
    nx::network::http::HttpHeaders headers;
    headers.emplace(nx::network::http::header::kContentType, "application/json");

    return headers;
}

} // namespace

//-------------------------------------------------------------------------------------------------
// QnMediaServerReplyProcessor

QnMediaServerReplyProcessor::QnMediaServerReplyProcessor(int object, const QString& serverId):
    QnAbstractReplyProcessor(object),
    m_serverId(serverId)
{
    timer.start();
}

void QnMediaServerReplyProcessor::processReply(const QnHTTPRawResponse& response, int handle)
{
    trace(m_serverId, handle, object(), lit("Received reply (%1ms)").arg(timer.elapsed()));
    switch (object())
    {
        case StorageStatusObject:
            processJsonReply<QnStorageStatusReply>(this, response, handle);
            break;
        case StorageSpaceObject:
            processJsonReply<QnStorageSpaceReply>(this, response, handle);
            break;
        case GetParamsObject:
        case SetParamsObject:
            processJsonReply<QnCameraAdvancedParamValueList>(this, response, handle);
            break;
        case TestEmailSettingsObject:
            processJsonReply<QnTestEmailSettingsReply>(this, response, handle);
            break;
        case CameraAddObject:
            emitFinished(this, response.status, handle);
            break;
        case checkCamerasObject:
            processJsonReply<QnCameraListReply>(this, response, handle);
            break;
        case CameraDiagnosticsObject:
            processJsonReply<QnCameraDiagnosticsReply>(this, response, handle);
            break;
        case GetSystemIdObject:
            emitFinished(this, response.status, QString::fromUtf8(response.msgBody), handle);
            break;
        case RebuildArchiveObject:
            processJsonReply<QnStorageScanData>(this, response, handle);
            break;
        case BackupControlObject:
            processJsonReply<QnBackupStatusData>(this, response, handle);
            break;
        case PingSystemObject:
            processJsonReply<nx::vms::api::ModuleInformation>(this, response, handle);
            break;
        case GetNonceObject:
            processJsonReply<QnGetNonceReply>(this, response, handle);
            break;
        case RecordingStatsObject:
            processJsonReply<QnRecordingStatsReply>(this, response, handle);
            break;
        case AuditLogObject:
            processUbjsonReply<QnAuditRecordList>(this, response, handle);
            break;
        case ec2RecordedTimePeriodsObject:
            processCompressedPeriodsReply<MultiServerPeriodDataList>(this, response, handle);
            break;
        case TestLdapSettingsObject:
            processJsonReply<QnLdapUsers>(this, response, handle);
            break;
        default:
            NX_ASSERT(false);
            break;
    }

    deleteLater();
}

//-------------------------------------------------------------------------------------------------
// QnMediaServerConnection

QnMediaServerConnection::QnMediaServerConnection(
    QnCommonModule* commonModule,
    const QnMediaServerResourcePtr& server,
    const QnUuid& videowallGuid,
    bool enableOfflineRequests)
:
    base_type(commonModule, server),
    m_server(server),
    m_serverVersion(server->getVersion()),
    m_proxyPort(0),
    m_enableOfflineRequests(enableOfflineRequests)
{
    setSerializer(QnLexical::newEnumSerializer<RequestObject, int>());
    setExtraQueryParameters(QnRequestParamList{{Qn::SERVER_GUID_HEADER_NAME, server->getId().toString()}});

    nx::network::http::HttpHeaders extraHeaders;
    extraHeaders.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getOriginalGuid().toByteArray());

    if (!videowallGuid.isNull())
        extraHeaders.emplace(Qn::VIDEOWALL_GUID_HEADER_NAME, videowallGuid.toByteArray());
    extraHeaders.emplace(Qn::EC2_RUNTIME_GUID_HEADER_NAME,
        commonModule->runningInstanceGUID().toByteArray());
    if (const auto& connection = commonModule->ec2Connection())
    {
        extraHeaders.emplace(Qn::CUSTOM_USERNAME_HEADER_NAME,
            connection->connectionInfo().ecUrl.userName().toUtf8());
    }
    extraHeaders.emplace(nx::network::http::header::kUserAgent, nx::network::http::userAgentString());
    setExtraHeaders(std::move(extraHeaders));
}

nx::utils::Url QnMediaServerConnection::url() const
{
    if (const auto server = m_server.lock())
        return server->getApiUrl();

    return nx::utils::Url();
}

QnAbstractReplyProcessor* QnMediaServerConnection::newReplyProcessor(int object, const QString& serverId)
{
    return new QnMediaServerReplyProcessor(object, serverId);
}

bool QnMediaServerConnection::isReady() const
{
    if (!targetResource())
        return false;

    if (m_enableOfflineRequests)
        return true;

    Qn::ResourceStatus status = targetResource()->getStatus();
    return status != Qn::Offline && status != Qn::NotDefined;
}

int QnMediaServerConnection::sendAsyncGetRequestLogged(
    int object,
    const QnRequestParamList& params,
    const char* replyTypeName,
    QObject* target,
    const char* slot,
    std::optional<std::chrono::milliseconds> timeout)
{
    int handle = sendAsyncGetRequest(object, params, replyTypeName, target, slot, timeout);

    trace(handle, object, lm("GET %1 with timeout %2").args(
        replyTypeName, timeout ? *timeout : std::chrono::milliseconds{0}));

    return handle;
}

int QnMediaServerConnection::sendAsyncPostRequestLogged(
    int object,
    const QnRequestParamList& params,
    const char* replyTypeName,
    QObject* target,
    const char* slot,
    std::optional<std::chrono::milliseconds> timeout)
{
    return sendAsyncPostRequestLogged(
        object, makeDefaultHeaders(), {}, QJson::serialized(params.toJson()),
        replyTypeName, target, slot, timeout);
}

int QnMediaServerConnection::sendAsyncPostRequestLogged(
    int object,
    nx::network::http::HttpHeaders headers,
    const QnRequestParamList& params,
    const QByteArray& data,
    const char* replyTypeName,
    QObject* target,
    const char* slot,
    std::optional<std::chrono::milliseconds> timeout)
{
    int handle = sendAsyncPostRequest(
        object, std::move(headers), params, data, replyTypeName, target, slot, timeout);

    trace(handle, object, lm("POST %1 with timeout %2").args(
        replyTypeName, timeout ? *timeout : std::chrono::milliseconds{0}));

    return handle;
}

void QnMediaServerConnection::trace(int handle, int obj, const QString& message /*= QString()*/)
{
    ::trace(m_serverId, handle, obj, message);
}

int QnMediaServerConnection::checkCameraList(
    const QnNetworkResourceList& cameras, QObject* target, const char* slot)
{
    QnCameraListReply camList;
    for (const QnResourcePtr& c: cameras)
        camList.uniqueIdList << c->getUniqueId();

    nx::network::http::HttpHeaders headers;
    headers.emplace(nx::network::http::header::kContentType, "application/json");

    return sendAsyncPostRequestLogged(checkCamerasObject,
        std::move(headers), QnRequestParamList(), QJson::serialized(camList),
        QN_STRINGIZE_TYPE(QnCameraListReply), target, slot);
}

int QnMediaServerConnection::getParamsAsync(
    const QnNetworkResourcePtr& camera, const QStringList& keys, QObject* target, const char* slot)
{
    NX_ASSERT(!keys.isEmpty(), "parameter names should be provided");

    QnRequestParamList params;
    params.insert("cameraId", camera->getId());
    for(const QString &param: keys)
        params.insert(param, QString());

    return sendAsyncGetRequestLogged(GetParamsObject,
        params, QN_STRINGIZE_TYPE(QnCameraAdvancedParamValueList), target, slot);
}

int QnMediaServerConnection::setParamsAsync(
    const QnNetworkResourcePtr& camera, const QnCameraAdvancedParamValueList& values,
    QObject* target, const char* slot)
{
    NX_ASSERT(!values.isEmpty(), "parameter names should be provided");

    QnCameraAdvancedParamsPostBody postBody;
    postBody.cameraId = camera->getId().toString();
    for(const auto& value: values)
        postBody.paramValues.insert(value.id, value.value);

    return sendAsyncPostRequestLogged(SetParamsObject,
        makeDefaultHeaders(), QnRequestParamList(), QJson::serialized(postBody),
        QN_STRINGIZE_TYPE(QnCameraAdvancedParamValueList), target, slot);
}

int QnMediaServerConnection::addCameraAsync(
    const QnManualResourceSearchList& cameras, const QString& username, const QString& password,
    QObject* target, const char* slot) {
    QnRequestParamList params;
    for (int i = 0; i < cameras.size(); i++){
        params.insert(lit("url") + QString::number(i), cameras[i].url);
        params.insert(lit("manufacturer") + QString::number(i), cameras[i].manufacturer);
        params.insert(lit("uniqueId") + QString::number(i), cameras[i].uniqueId);
    }
    params.insert("user", username);
    params.insert("password", password);

    return sendAsyncPostRequestLogged(CameraAddObject, params, nullptr, target, slot);
}

int QnMediaServerConnection::getSystemIdAsync(QObject* target, const char* slot)
{
    return sendAsyncGetRequestLogged(GetSystemIdObject,
        QnRequestParamList(), QN_STRINGIZE_TYPE(QString), target, slot);
}

int QnMediaServerConnection::testEmailSettingsAsync(
    const QnEmailSettings& settings, QObject* target, const char* slot)
{
    nx::network::http::HttpHeaders headers;
    headers.emplace(nx::network::http::header::kContentType, "application/json");

    nx::vms::api::EmailSettingsData data;
    ec2::fromResourceToApi(settings, data);
    return sendAsyncPostRequestLogged(TestEmailSettingsObject,
        std::move(headers), QnRequestParamList(), QJson::serialized(data),
        QN_STRINGIZE_TYPE(QnTestEmailSettingsReply), target, slot);
}

int QnMediaServerConnection::testLdapSettingsAsync(
    const QnLdapSettings& settings, QObject* target, const char* slot)
{
    nx::network::http::HttpHeaders headers;
    headers.emplace(nx::network::http::header::kContentType, "application/json");
    std::chrono::seconds timeout(settings.searchTimeoutS);
    return sendAsyncPostRequestLogged(TestLdapSettingsObject, std::move(headers),
        QnRequestParamList(), QJson::serialized(settings),
        QN_STRINGIZE_TYPE(QnLdapUsers), target, slot, timeout);
}

int QnMediaServerConnection::doCameraDiagnosticsStepAsync(
    const QnUuid& cameraId, CameraDiagnostics::Step::Value previousStep, QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    params.insert("cameraId", cameraId);
    params.insert("type", CameraDiagnostics::Step::toString(previousStep));
    return sendAsyncGetRequestLogged(CameraDiagnosticsObject,
        params, QN_STRINGIZE_TYPE(QnCameraDiagnosticsReply), target, slot);
}

int QnMediaServerConnection::doRebuildArchiveAsync(
    Qn::RebuildAction action, bool isMainPool, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params.insert("action", QnLexical::serialized(action));
    params.insert("mainPool", isMainPool);
    return sendAsyncPostRequestLogged(RebuildArchiveObject,
        params, QN_STRINGIZE_TYPE(QnStorageScanData), target, slot);
}

int QnMediaServerConnection::backupControlActionAsync(
    Qn::BackupAction action, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params.insert("action", QnLexical::serialized(action));
    return sendAsyncPostRequestLogged(BackupControlObject,
        params, QN_STRINGIZE_TYPE(QnBackupStatusData), target, slot);
}

int QnMediaServerConnection::getStorageSpaceAsync(
    bool fastRequest, QObject* target, const char* slot)
{
    QnRequestParamList params;
    if (fastRequest)
        params.insert("fast", QnLexical::serialized(true));
    return sendAsyncGetRequestLogged(StorageSpaceObject,
        params, QN_STRINGIZE_TYPE(QnStorageSpaceReply), target, slot);
}

int QnMediaServerConnection::getStorageStatusAsync(
    const QString& storageUrl, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params.insert("path", storageUrl);
    return sendAsyncGetRequestLogged(StorageStatusObject,
        params, QN_STRINGIZE_TYPE(QnStorageStatusReply), target, slot);
}

int QnMediaServerConnection::pingSystemAsync(
    const nx::utils::Url& url, const QString& getKey, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params.insert("url", url.toString());
    params.insert("getKey", getKey);

    return sendAsyncGetRequestLogged(PingSystemObject,
        params, QN_STRINGIZE_TYPE(nx::vms::api::ModuleInformation), target, slot);
}

int QnMediaServerConnection::getNonceAsync(const nx::utils::Url& url, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params.insert("url", url.toString());

    return sendAsyncGetRequest(GetNonceObject,
        params, QN_STRINGIZE_TYPE(QnGetNonceReply), target, slot);
}

int QnMediaServerConnection::getRecordingStatisticsAsync(
    qint64 bitrateAnalyzePeriodMs, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params.insert("bitrateAnalyzePeriodMs", bitrateAnalyzePeriodMs);
    return sendAsyncGetRequestLogged(RecordingStatsObject,
        params, QN_STRINGIZE_TYPE(QnRecordingStatsReply), target, slot);
}

int QnMediaServerConnection::getAuditLogAsync(
    qint64 startTimeMs, qint64 endTimeMs, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params.insert("from", startTimeMs * 1000ll);
    params.insert("to", endTimeMs * 1000ll);
    params.insert("format", "ubjson");
    return sendAsyncGetRequest(AuditLogObject,
        params, QN_STRINGIZE_TYPE(QnAuditRecordList), target, slot);
}

int QnMediaServerConnection::recordedTimePeriods(
    const QnChunksRequestData& request, QObject* target, const char* slot)
{
    nx::utils::SoftwareVersion connectionVersion;
    if (const auto& connection = commonModule()->ec2Connection())
        connectionVersion = connection->connectionInfo().version;

    QnChunksRequestData fixedFormatRequest(request);
    fixedFormatRequest.format = Qn::CompressedPeriodsFormat;

    if (!connectionVersion.isNull() && connectionVersion < nx::utils::SoftwareVersion(3, 0))
        fixedFormatRequest.requestVersion = QnChunksRequestData::RequestVersion::v2_6;

    return sendAsyncGetRequestLogged(ec2RecordedTimePeriodsObject,
        fixedFormatRequest.toParams(), QN_STRINGIZE_TYPE(MultiServerPeriodDataList), target, slot);
}
