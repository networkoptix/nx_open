#include "media_server_connection.h"

#include <QtCore/QSharedPointer>
#include <QtCore/QUrl>

#include <QtNetwork/QNetworkReply>

#include <nx/utils/uuid.h>

#include <api/helpers/chunks_request_data.h>
#include <api/helpers/bookmark_request_data.h>

#include <core/resource/camera_advanced_param.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_bookmark.h>

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
    (TimePeriodsObject, "RecordedTimePeriods")
    (PtzContinuousMoveObject, "ptz")
    (PtzContinuousFocusObject, "ptz")
    (PtzAbsoluteMoveObject, "ptz")
    (PtzViewportMoveObject, "ptz")
    (PtzRelativeMoveObject, "ptz")
    (PtzRelativeFocusObject, "ptz")
    (PtzGetPositionObject, "ptz")
    (PtzCreatePresetObject, "ptz")
    (PtzUpdatePresetObject, "ptz")
    (PtzRemovePresetObject, "ptz")
    (PtzActivatePresetObject, "ptz")
    (PtzGetPresetsObject, "ptz")
    (PtzCreateTourObject, "ptz")
    (PtzRemoveTourObject, "ptz")
    (PtzActivateTourObject, "ptz")
    (PtzGetToursObject, "ptz")
    (PtzGetHomeObjectObject, "ptz")
    (PtzGetActiveObjectObject, "ptz")
    (PtzUpdateHomeObjectObject, "ptz")
    (PtzGetDataObject, "ptz")
    (PtzGetAuxiliaryTraitsObject, "ptz")
    (PtzRunAuxiliaryCommandObject, "ptz")
    (GetParamsObject, "getCameraParam")
    (SetParamsObject, "setCameraParam")
    (TimeObject, "gettime")
    (CameraSearchStartObject, "manualCamera/search")
    (CameraSearchStatusObject, "manualCamera/status")
    (CameraSearchStopObject, "manualCamera/stop")
    (CameraAddObject, "manualCamera/add")
    (checkCamerasObject, "checkDiscovery")
    (CameraDiagnosticsObject, "doCameraDiagnosticsStep")
    (GetSystemIdObject, "getSystemId")
    (RebuildArchiveObject, "rebuildArchive")
    (BackupControlObject, "backupControl")
    (BookmarkAddObject, "cameraBookmarks/add")
    (BookmarkUpdateObject, "cameraBookmarks/update")
    (BookmarkDeleteObject, "cameraBookmarks/delete")
    (InstallUpdateObject, "installUpdate")
    (InstallUpdateUnauthenticatedObject, "installUpdateUnauthenticated")
    (Restart, "restart")
    (ConfigureObject, "configure")
    (PingSystemObject, "pingSystem")
    (GetNonceObject, "getRemoteNonce")
    (RecordingStatsObject, "recStats")
    (AuditLogObject, "auditLog")
    (MergeSystemsObject, "mergeSystems")
    (TestEmailSettingsObject, "testEmailSettings")
    (TestLdapSettingsObject, "testLdapSettings")
    (ModulesInformationObject, "moduleInformation")
    (ec2RecordedTimePeriodsObject, "ec2/recordedTimePeriods")
    (ec2BookmarksObject, "ec2/bookmarks")
    (ec2BookmarkAddObject, "ec2/bookmarks/add")
    (ec2BookmarkAcknowledgeObject, "ec2/bookmarks/acknowledge")
    (ec2BookmarkUpdateObject, "ec2/bookmarks/update")
    (ec2BookmarkDeleteObject, "ec2/bookmarks/delete")
    (ec2BookmarkTagsObject, "ec2/bookmarks/tags")
    (MergeLdapUsersObject, "mergeLdapUsers")
);

#if 0
QByteArray extractXmlBody(const QByteArray& body, const QByteArray& tagName, int* from = nullptr)
{
    QByteArray tagStart = QByteArray("<") + tagName + QByteArray(">");
    int bodyStart = body.indexOf(tagStart, from ? *from : 0);
    if (bodyStart >= 0)
        bodyStart += tagStart.length();
    QByteArray tagEnd = QByteArray("</") + tagName + QByteArray(">");
    int bodyEnd = body.indexOf(tagEnd, bodyStart);
    if (bodyStart >= 0 && bodyEnd >= 0)
    {
        if (from)
            *from = bodyEnd + tagEnd.length();
        return body.mid(bodyStart, bodyEnd - bodyStart).trimmed();
    }
    return QByteArray();
}
#endif // 0

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
        case TimePeriodsObject:
        {
            int status = response.status;

            QnTimePeriodList reply;
            if (status == 0)
            {
                if (response.msgBody.startsWith("BIN"))
                {
                    reply.decode(
                        (const quint8*) response.msgBody.constData() + 3,
                        response.msgBody.size() - 3);
                }
                else
                {
                    qWarning() << "QnMediaServerConnection: unexpected message received.";
                    status = -1;
                }
            }

            emitFinished(this, status, reply, handle);
            break;
        }
        case GetParamsObject:
        case SetParamsObject:
            processJsonReply<QnCameraAdvancedParamValueList>(this, response, handle);
            break;
        case TimeObject:
            processJsonReply<QnTimeReply>(this, response, handle);
            break;
        case TestEmailSettingsObject:
            processJsonReply<QnTestEmailSettingsReply>(this, response, handle);
            break;
        case CameraAddObject:
            emitFinished(this, response.status, handle);
            break;
        case PtzContinuousMoveObject:
        case PtzContinuousFocusObject:
        case PtzAbsoluteMoveObject:
        case PtzViewportMoveObject:
        case PtzRelativeMoveObject:
        case PtzRelativeFocusObject:
        case PtzCreatePresetObject:
        case PtzUpdatePresetObject:
        case PtzRemovePresetObject:
        case PtzActivatePresetObject:
        case PtzCreateTourObject:
        case PtzRemoveTourObject:
        case PtzActivateTourObject:
        case PtzUpdateHomeObjectObject:
        case PtzRunAuxiliaryCommandObject:
            emitFinished(this, response.status, handle);
            break;
        case PtzGetPositionObject:
            processJsonReply<QVector3D>(this, response, handle);
            break;
        case PtzGetPresetsObject:
            processJsonReply<QnPtzPresetList>(this, response, handle);
            break;
        case PtzGetToursObject:
            processJsonReply<QnPtzTourList>(this, response, handle);
            break;
        case PtzGetActiveObjectObject:
        case PtzGetHomeObjectObject:
            processJsonReply<QnPtzObject>(this, response, handle);
            break;
        case PtzGetAuxiliaryTraitsObject:
            processJsonReply<QnPtzAuxiliaryTraitList>(this, response, handle);
            break;
        case PtzGetDataObject:
            processJsonReply<QnPtzData>(this, response, handle);
            break;
        case CameraSearchStartObject:
        case CameraSearchStatusObject:
        case CameraSearchStopObject:
            processJsonReply<QnManualCameraSearchReply>(this, response, handle);
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
        case ec2BookmarkAddObject:
        case ec2BookmarkAcknowledgeObject:
        case ec2BookmarkUpdateObject:
        case ec2BookmarkDeleteObject:
            emitFinished(this, response.status, handle);
            break;
        case InstallUpdateObject:
        case InstallUpdateUnauthenticatedObject:
            processJsonReply<QnUploadUpdateReply>(this, response, handle);
            break;
        case Restart:
            emitFinished(this, response.status, handle);
            break;
        case ConfigureObject:
            processJsonReply<QnConfigureReply>(this, response, handle);
            break;
        case ModulesInformationObject:
            processJsonReply<QList<nx::vms::api::ModuleInformation>>(this, response, handle);
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
        case MergeSystemsObject:
            processJsonReply<nx::vms::api::ModuleInformation>(this, response, handle);
            break;
        case ec2RecordedTimePeriodsObject:
            processCompressedPeriodsReply<MultiServerPeriodDataList>(this, response, handle);
            break;
        case ec2BookmarksObject:
            processFusionReply<QnCameraBookmarkList>(this, response, handle);
            break;
        case ec2BookmarkTagsObject:
            processFusionReply<QnCameraBookmarkTagList>(this, response, handle);
            break;
        case TestLdapSettingsObject:
            processJsonReply<QnLdapUsers>(this, response, handle);
            break;
        case MergeLdapUsersObject:
            processJsonReply(this, response, handle);
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
    const QnMediaServerResourcePtr& mserver,
    const QnUuid& videowallGuid,
    bool enableOfflineRequests)
:
    base_type(commonModule, mserver),
    m_serverVersion(mserver->getVersion()),
    m_proxyPort(0),
    m_enableOfflineRequests(enableOfflineRequests)
{
    setSerializer(QnLexical::newEnumSerializer<RequestObject, int>());
    nx::network::http::HttpHeaders extraHeaders;

    if (mserver)
    {
        setUrl(mserver->getApiUrl());

        QnRequestParamList queryParameters;
        queryParameters.insert(QString::fromLatin1(Qn::SERVER_GUID_HEADER_NAME),
            mserver->getId().toString());
        setExtraQueryParameters(std::move(queryParameters));

        extraHeaders.emplace(Qn::SERVER_GUID_HEADER_NAME,
            mserver->getOriginalGuid().toByteArray());
    }

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

QnMediaServerConnection::~QnMediaServerConnection()
{
    return;
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

int QnMediaServerConnection::getTimePeriodsAsync(
    const QnVirtualCameraResourcePtr& camera, qint64 startTimeMs, qint64 endTimeMs, qint64 detail,
    Qn::TimePeriodContent periodsType, const QString& filter, QObject* target, const char* slot)
{
    QnRequestParamList params;

    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("startTime", QString::number(startTimeMs));
    params << QnRequestParam("endTime", QString::number(endTimeMs));
    params << QnRequestParam("detail", QString::number(detail));
    params << QnRequestParam("format", "bin");
    params << QnRequestParam("periodsType", QString::number(static_cast<int>(periodsType)));
    params << QnRequestParam("filter", filter);

    #if defined(QN_MEDIA_SERVER_API_DEBUG)
        QString path = url().toDisplayString() + lit("/api/RecordedTimePeriods?t=1");
        for (const QnRequestParam& param: params)
            path += L'&' + param.first + L'=' + param.second;
        qDebug() << "Requesting chunks" << path;
    #endif

    return sendAsyncGetRequestLogged(TimePeriodsObject,
        params, QN_STRINGIZE_TYPE(QnTimePeriodList), target, slot);
}

int QnMediaServerConnection::getParamsAsync(
    const QnNetworkResourcePtr& camera, const QStringList& keys, QObject* target, const char* slot)
{
    NX_ASSERT(!keys.isEmpty(), "parameter names should be provided");

    QnRequestParamList params;
    params << QnRequestParam("cameraId", camera->getId());
    for(const QString &param: keys)
        params << QnRequestParam(param, QString());

    return sendAsyncGetRequestLogged(GetParamsObject,
        params, QN_STRINGIZE_TYPE(QnCameraAdvancedParamValueList), target, slot);
}

int QnMediaServerConnection::setParamsAsync(
    const QnNetworkResourcePtr& camera, const QnCameraAdvancedParamValueList& values,
    QObject* target, const char* slot)
{
    NX_ASSERT(!values.isEmpty(), "parameter names should be provided");

    QnRequestParamList params;
    params << QnRequestParam("cameraId", camera->getId());
    for(const QnCameraAdvancedParamValue value: values)
        params << QnRequestParam(value.id, value.value);

    // TODO: Change GET to POST when compatibility with old servers is not needed anymore.
    return sendAsyncGetRequestLogged(SetParamsObject,
        params, QN_STRINGIZE_TYPE(QnCameraAdvancedParamValueList), target, slot);
}

int QnMediaServerConnection::searchCameraAsyncStart(
    const QString& startAddr, const QString& endAddr, const QString& username,
    const QString& password, int port, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("start_ip", startAddr);
    if (!endAddr.isEmpty())
        params << QnRequestParam("end_ip", endAddr);
    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);
    params << QnRequestParam("port", QString::number(port));

    return sendAsyncGetRequestLogged(CameraSearchStartObject,
        params, QN_STRINGIZE_TYPE(QnManualCameraSearchReply), target, slot);
}

int QnMediaServerConnection::searchCameraAsyncStatus(
    const QnUuid& processUuid, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("uuid", processUuid.toString());
    return sendAsyncGetRequestLogged(CameraSearchStatusObject,
        params, QN_STRINGIZE_TYPE(QnManualCameraSearchReply), target, slot);
}

int QnMediaServerConnection::searchCameraAsyncStop(
    const QnUuid& processUuid, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("uuid", processUuid.toString());
    return sendAsyncGetRequestLogged(CameraSearchStopObject,
        params, QN_STRINGIZE_TYPE(QnManualCameraSearchReply), target, slot);
}

int QnMediaServerConnection::addCameraAsync(
    const QnManualResourceSearchList& cameras, const QString& username, const QString& password,
    QObject* target, const char* slot) {
    QnRequestParamList params;
    for (int i = 0; i < cameras.size(); i++){
        params << QnRequestParam(lit("url") + QString::number(i), cameras[i].url);
        params << QnRequestParam(lit("manufacturer") + QString::number(i), cameras[i].manufacturer);
        params << QnRequestParam(lit("uniqueId") + QString::number(i), cameras[i].uniqueId);
    }
    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);

    return sendAsyncGetRequestLogged(CameraAddObject, params, nullptr, target, slot);
}

void QnMediaServerConnection::addOldVersionPtzParams(
    const QnNetworkResourcePtr& camera,
    QnRequestParamList& params)
{
    if (m_serverVersion < nx::utils::SoftwareVersion(3, 0))
        params << QnRequestParam("resourceId", QnLexical::serialized(camera->getUniqueId()));
}

int QnMediaServerConnection::ptzContinuousMoveAsync(
    const QnNetworkResourcePtr& camera,
    const nx::core::ptz::Vector& speed,
    const nx::core::ptz::Options& options,
    const QnUuid& sequenceId,
    int sequenceNumber,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::ContinuousMovePtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("xSpeed", QnLexical::serialized(speed.pan));
    params << QnRequestParam("ySpeed", QnLexical::serialized(speed.tilt));
    params << QnRequestParam("zSpeed", QnLexical::serialized(speed.zoom));
    params << QnRequestParam("rotationSpeed", QnLexical::serialized(speed.rotation));
    params << QnRequestParam("type", QnLexical::serialized(options.type));
    params << QnRequestParam("sequenceId", sequenceId);
    params << QnRequestParam("sequenceNumber", sequenceNumber);

    return sendAsyncPostRequestLogged(
        PtzContinuousMoveObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzContinuousFocusAsync(
    const QnNetworkResourcePtr& camera,
    qreal speed,
    const nx::core::ptz::Options& options,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::ContinuousFocusPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("speed", QnLexical::serialized(speed));
    params << QnRequestParam("type", QnLexical::serialized(options.type));

    return sendAsyncPostRequestLogged(
        PtzContinuousFocusObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzAbsoluteMoveAsync(
    const QnNetworkResourcePtr& camera,
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Vector& position,
    qreal speed,
    const nx::core::ptz::Options& options,
    const QnUuid& sequenceId,
    int sequenceNumber,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(
        space == Qn::DevicePtzCoordinateSpace
        ? Qn::AbsoluteDeviceMovePtzCommand
        : Qn::AbsoluteLogicalMovePtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("xPos", QnLexical::serialized(position.pan));
    params << QnRequestParam("yPos", QnLexical::serialized(position.tilt));
    params << QnRequestParam("zPos", QnLexical::serialized(position.zoom));
    params << QnRequestParam("rotaion", QnLexical::serialized(position.rotation));
    params << QnRequestParam("speed", QnLexical::serialized(speed));
    params << QnRequestParam("type", QnLexical::serialized(options.type));
    params << QnRequestParam("sequenceId", sequenceId);
    params << QnRequestParam("sequenceNumber", sequenceNumber);

    return sendAsyncPostRequestLogged(
        PtzAbsoluteMoveObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzViewportMoveAsync(
    const QnNetworkResourcePtr& camera,
    qreal aspectRatio,
    const QRectF& viewport,
    qreal speed,
    const nx::core::ptz::Options& options,
    const QnUuid& sequenceId,
    int sequenceNumber,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::ViewportMovePtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("aspectRatio", QnLexical::serialized(aspectRatio));
    params << QnRequestParam("viewportTop", QnLexical::serialized(viewport.top()));
    params << QnRequestParam("viewportLeft", QnLexical::serialized(viewport.left()));
    params << QnRequestParam("viewportBottom", QnLexical::serialized(viewport.bottom()));
    params << QnRequestParam("viewportRight", QnLexical::serialized(viewport.right()));
    params << QnRequestParam("speed", QnLexical::serialized(speed));
    params << QnRequestParam("type", QnLexical::serialized(options.type));
    params << QnRequestParam("sequenceId", sequenceId);
    params << QnRequestParam("sequenceNumber", sequenceNumber);

    return sendAsyncPostRequestLogged(
        PtzViewportMoveObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzRelativeMoveAsync(
    const QnNetworkResourcePtr& camera,
    const core::ptz::Vector& direction,
    const core::ptz::Options& options,
    const QnUuid& sequenceId,
    int sequenceNumber,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("command", QnLexical::serialized(Qn::RelativeMovePtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("pan", QnLexical::serialized(direction.pan));
    params << QnRequestParam("tilt", QnLexical::serialized(direction.tilt));
    params << QnRequestParam("rotation", QnLexical::serialized(direction.rotation));
    params << QnRequestParam("zoom", QnLexical::serialized(direction.zoom));
    params << QnRequestParam("type", QnLexical::serialized(options.type));
    params << QnRequestParam("sequenceId", sequenceId);
    params << QnRequestParam("sequenceNumber", sequenceNumber);

    return sendAsyncPostRequestLogged(
        PtzRelativeMoveObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzRelativeFocusAsync(
    const QnNetworkResourcePtr& camera,
    qreal direction,
    const core::ptz::Options& options,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("command", QnLexical::serialized(Qn::RelativeFocusPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("focus", QnLexical::serialized(direction));
    params << QnRequestParam("type", QnLexical::serialized(options.type));

    return sendAsyncPostRequestLogged(
        PtzRelativeFocusObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzGetPositionAsync(
    const QnNetworkResourcePtr& camera,
    Qn::PtzCoordinateSpace space,
    const nx::core::ptz::Options& options,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(
        space == Qn::DevicePtzCoordinateSpace
            ? Qn::GetDevicePositionPtzCommand
            : Qn::GetLogicalPositionPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("type", QnLexical::serialized(options.type));

    return sendAsyncPostRequestLogged(
        PtzGetPositionObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        QN_STRINGIZE_TYPE(QVector3D),
        target,
        slot);
}

int QnMediaServerConnection::ptzCreatePresetAsync(
    const QnNetworkResourcePtr& camera,
    const QnPtzPreset& preset,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::CreatePresetPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("presetName", preset.name);
    params << QnRequestParam("presetId", preset.id);

    return sendAsyncPostRequestLogged(
        PtzCreatePresetObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzUpdatePresetAsync(
    const QnNetworkResourcePtr& camera,
    const QnPtzPreset& preset,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::UpdatePresetPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("presetName", preset.name);
    params << QnRequestParam("presetId", preset.id);

    return sendAsyncPostRequestLogged(
        PtzUpdatePresetObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzRemovePresetAsync(
    const QnNetworkResourcePtr& camera,
    const QString& presetId,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::RemovePresetPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("presetId", presetId);

    return sendAsyncPostRequestLogged(
        PtzRemovePresetObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzActivatePresetAsync(
    const QnNetworkResourcePtr& camera,
    const QString& presetId,
    qreal speed,
    QObject* target,
    const char *slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::ActivatePresetPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("presetId", presetId);
    params << QnRequestParam("speed", QnLexical::serialized(speed));

    return sendAsyncPostRequest(
        PtzActivatePresetObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzGetPresetsAsync(
    const QnNetworkResourcePtr& camera,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::GetPresetsPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());

    return sendAsyncPostRequestLogged(
        PtzGetPresetsObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        QN_STRINGIZE_TYPE(QnPtzPresetList),
        target,
        slot);
}

int QnMediaServerConnection::ptzCreateTourAsync(
    const QnNetworkResourcePtr& camera,
    const QnPtzTour& tour,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::CreateTourPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());

    return sendAsyncPostRequestLogged(
        PtzCreateTourObject,
        makeDefaultHeaders(),
        params,
        QJson::serialized(tour),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzRemoveTourAsync(
    const QnNetworkResourcePtr& camera,
    const QString& tourId,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::RemoveTourPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("tourId", tourId);

    return sendAsyncPostRequestLogged(
        PtzRemoveTourObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzActivateTourAsync(
    const QnNetworkResourcePtr& camera,
    const QString& tourId,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::ActivateTourPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("tourId", tourId);

    return sendAsyncPostRequestLogged(
        PtzActivateTourObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzGetToursAsync(
    const QnNetworkResourcePtr& camera,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::GetToursPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());

    return sendAsyncPostRequestLogged(
        PtzGetToursObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        QN_STRINGIZE_TYPE(QnPtzTourList),
        target,
        slot);
}

int QnMediaServerConnection::ptzGetActiveObjectAsync(
    const QnNetworkResourcePtr& camera,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::GetActiveObjectPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());

    return sendAsyncPostRequestLogged(
        PtzGetActiveObjectObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        QN_STRINGIZE_TYPE(QnPtzObject),
        target,
        slot);
}

int QnMediaServerConnection::ptzUpdateHomeObjectAsync(
    const QnNetworkResourcePtr& camera,
    const QnPtzObject& homePosition,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::UpdateHomeObjectPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("objectType", QnLexical::serialized(homePosition.type));
    params << QnRequestParam("objectId", homePosition.id);

    return sendAsyncPostRequestLogged(
        PtzUpdateHomeObjectObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        QN_STRINGIZE_TYPE(QnPtzObject),
        target,
        slot);
}

int QnMediaServerConnection::ptzGetHomeObjectAsync(
    const QnNetworkResourcePtr& camera,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::GetHomeObjectPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());

    return sendAsyncPostRequestLogged(
        PtzGetHomeObjectObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        QN_STRINGIZE_TYPE(QnPtzObject),
        target,
        slot);
}

int QnMediaServerConnection::ptzGetAuxiliaryTraitsAsync(
    const QnNetworkResourcePtr& camera,
    const nx::core::ptz::Options& options,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::GetAuxiliaryTraitsPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("type", QnLexical::serialized(options.type));

    return sendAsyncPostRequestLogged(
        PtzGetAuxiliaryTraitsObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        QN_STRINGIZE_TYPE(QnPtzAuxiliaryTraitList),
        target,
        slot);
}

int QnMediaServerConnection::ptzRunAuxiliaryCommandAsync(
    const QnNetworkResourcePtr& camera,
    const QnPtzAuxiliaryTrait& trait,
    const QString& data,
    const nx::core::ptz::Options& options,
    QObject* target, const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::RunAuxiliaryCommandPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("trait", QnLexical::serialized(trait));
    params << QnRequestParam("data", data);
    params << QnRequestParam("type", QnLexical::serialized(options.type));

    return sendAsyncPostRequestLogged(
        PtzRunAuxiliaryCommandObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        nullptr,
        target,
        slot);
}

int QnMediaServerConnection::ptzGetDataAsync(
    const QnNetworkResourcePtr& camera,
    Qn::PtzDataFields query,
    const nx::core::ptz::Options& options,
    QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    addOldVersionPtzParams(camera, params);
    params << QnRequestParam("command", QnLexical::serialized(Qn::GetDataPtzCommand));
    params << QnRequestParam("cameraId", camera->getId());
    params << QnRequestParam("query", QnLexical::serialized(query));
    params << QnRequestParam("type", QnLexical::serialized(options.type));

    return sendAsyncPostRequestLogged(
        PtzGetDataObject,
        makeDefaultHeaders(),
        params,
        nx::Buffer(),
        QN_STRINGIZE_TYPE(QnPtzData),
        target,
        slot);
}

int QnMediaServerConnection::getTimeAsync(QObject* target, const char* slot)
{
    return sendAsyncGetRequestLogged(TimeObject,
        QnRequestParamList(), QN_STRINGIZE_TYPE(QnTimeReply), target, slot);
}

int QnMediaServerConnection::mergeLdapUsersAsync(QObject* target, const char* slot)
{
    return sendAsyncGetRequestLogged(MergeLdapUsersObject,
        QnRequestParamList(), nullptr, target, slot);
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
    params << QnRequestParam("cameraId", cameraId);
    params << QnRequestParam("type", CameraDiagnostics::Step::toString(previousStep));
    return sendAsyncGetRequestLogged(CameraDiagnosticsObject,
        params, QN_STRINGIZE_TYPE(QnCameraDiagnosticsReply), target, slot);
}

int QnMediaServerConnection::doRebuildArchiveAsync(
    Qn::RebuildAction action, bool isMainPool, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("action", QnLexical::serialized(action));
    params << QnRequestParam("mainPool", isMainPool);
    return sendAsyncGetRequestLogged(RebuildArchiveObject,
        params, QN_STRINGIZE_TYPE(QnStorageScanData), target, slot);
}

int QnMediaServerConnection::backupControlActionAsync(
    Qn::BackupAction action, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("action", QnLexical::serialized(action));
    return sendAsyncGetRequestLogged(BackupControlObject,
        params, QN_STRINGIZE_TYPE(QnBackupStatusData), target, slot);
}

int QnMediaServerConnection::getStorageSpaceAsync(
    bool fastRequest, QObject* target, const char* slot)
{
    QnRequestParamList params;
    if (fastRequest)
        params << QnRequestParam("fast", QnLexical::serialized(true));
    return sendAsyncGetRequestLogged(StorageSpaceObject,
        params, QN_STRINGIZE_TYPE(QnStorageSpaceReply), target, slot);
}

int QnMediaServerConnection::getStorageStatusAsync(
    const QString& storageUrl, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("path", storageUrl);
    return sendAsyncGetRequestLogged(StorageStatusObject,
        params, QN_STRINGIZE_TYPE(QnStorageStatusReply), target, slot);
}

int QnMediaServerConnection::installUpdate(
    const QString& updateId, bool delayed, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("updateId", updateId);
    params << QnRequestParam("delayed", delayed);

    return sendAsyncGetRequestLogged(InstallUpdateObject,
        params, QN_STRINGIZE_TYPE(QnUploadUpdateReply), target, slot);
}

int QnMediaServerConnection::installUpdateUnauthenticated(
    const QString& updateId, bool delayed, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("updateId", updateId);
    params << QnRequestParam("delayed", delayed);

    return sendAsyncGetRequestLogged(InstallUpdateUnauthenticatedObject,
        params, QN_STRINGIZE_TYPE(QnUploadUpdateReply), target, slot);
}

int QnMediaServerConnection::uploadUpdateChunk(
    const QString& updateId, const QByteArray& data, qint64 offset, QObject* target,
    const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("updateId", updateId);
    params << QnRequestParam("offset", offset);

    nx::network::http::HttpHeaders headers;
    headers.emplace(nx::network::http::header::kContentType, "text/xml");

    return sendAsyncPostRequestLogged(InstallUpdateObject,
        std::move(headers), params, data, QN_STRINGIZE_TYPE(QnUploadUpdateReply), target, slot);
}

int QnMediaServerConnection::restart(QObject* target, const char* slot)
{
    return sendAsyncGetRequestLogged(Restart, QnRequestParamList(), nullptr, target, slot);
}

int QnMediaServerConnection::configureAsync(
    bool wholeSystem, const QString& systemName, const QString& password,
    const QByteArray& passwordHash, const QByteArray& passwordDigest,
    const QByteArray& cryptSha512Hash, int port, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("wholeSystem", wholeSystem ? lit("true") : lit("false"));
    params << QnRequestParam("systemName", systemName);
    params << QnRequestParam("password", password);
    params << QnRequestParam("passwordHash", QString::fromLatin1(passwordHash));
    params << QnRequestParam("passwordDigest", QString::fromLatin1(passwordDigest));
    params << QnRequestParam("cryptSha512Hash", QString::fromLatin1(cryptSha512Hash));
    params << QnRequestParam("port", port);

    return sendAsyncGetRequestLogged(ConfigureObject,
        params, QN_STRINGIZE_TYPE(QnConfigureReply), target, slot);
}

int QnMediaServerConnection::pingSystemAsync(
    const nx::utils::Url& url, const QString& getKey, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("url", url.toString());
    params << QnRequestParam("getKey", getKey);

    return sendAsyncGetRequestLogged(PingSystemObject,
        params, QN_STRINGIZE_TYPE(nx::vms::api::ModuleInformation), target, slot);
}

int QnMediaServerConnection::getNonceAsync(const nx::utils::Url& url, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("url", url.toString());

    return sendAsyncGetRequest(GetNonceObject,
        params, QN_STRINGIZE_TYPE(QnGetNonceReply), target, slot);
}

int QnMediaServerConnection::getRecordingStatisticsAsync(
    qint64 bitrateAnalyzePeriodMs, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("bitrateAnalyzePeriodMs", bitrateAnalyzePeriodMs);
    return sendAsyncGetRequestLogged(RecordingStatsObject,
        params, QN_STRINGIZE_TYPE(QnRecordingStatsReply), target, slot);
}

int QnMediaServerConnection::getAuditLogAsync(
    qint64 startTimeMs, qint64 endTimeMs, QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("from", startTimeMs * 1000ll);
    params << QnRequestParam("to", endTimeMs * 1000ll);
    params << QnRequestParam("format", "ubjson");
    return sendAsyncGetRequest(AuditLogObject,
        params, QN_STRINGIZE_TYPE(QnAuditRecordList), target, slot);
}

int QnMediaServerConnection::modulesInformation(QObject* target, const char* slot)
{
    QnRequestParamList params;
    params << QnRequestParam("allModules", lit("true"));
    return sendAsyncGetRequestLogged(ModulesInformationObject,
        params, QN_STRINGIZE_TYPE(QList<nx::vms::api::ModuleInformation>), target, slot);
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

int QnMediaServerConnection::acknowledgeEventAsync(
    const QnCameraBookmark& bookmark,
    const QnUuid& eventRuleId,
    QObject* target,
    const char* slot)
{
    QnUpdateBookmarkRequestData request(bookmark, eventRuleId);
    return sendAsyncGetRequestLogged(ec2BookmarkAcknowledgeObject,
        request.toParams(), nullptr, target, slot);
}

int QnMediaServerConnection::addBookmarkAsync(
    const QnCameraBookmark& bookmark,
    QObject* target,
    const char* slot)
{
    QnUpdateBookmarkRequestData request(bookmark);
    request.format = Qn::SerializationFormat::UbjsonFormat;

    return sendAsyncGetRequestLogged(ec2BookmarkAddObject, request.toParams(),
        nullptr, target, slot);
}

int QnMediaServerConnection::updateBookmarkAsync(
    const QnCameraBookmark& bookmark, QObject* target, const char* slot)
{
    QnUpdateBookmarkRequestData request(bookmark);
    request.format = Qn::SerializationFormat::UbjsonFormat;
    return sendAsyncGetRequestLogged(ec2BookmarkUpdateObject, request.toParams(), nullptr ,target, slot);
}

int QnMediaServerConnection::deleteBookmarkAsync(
    const QnUuid& bookmarkId, QObject* target, const char* slot)
{
    QnDeleteBookmarkRequestData request(bookmarkId);
    request.format = Qn::SerializationFormat::UbjsonFormat;
    return sendAsyncGetRequestLogged(ec2BookmarkDeleteObject, request.toParams(), nullptr ,target, slot);
}

int QnMediaServerConnection::getBookmarksAsync(
    const QnGetBookmarksRequestData& request, QObject* target, const char* slot)
{
    return sendAsyncGetRequestLogged(ec2BookmarksObject,
        request.toParams(), QN_STRINGIZE_TYPE(QnCameraBookmarkList), target, slot);
}

int QnMediaServerConnection::getBookmarkTagsAsync(
    const QnGetBookmarkTagsRequestData& request, QObject* target, const char* slot)
{
    return sendAsyncGetRequestLogged(ec2BookmarkTagsObject,
        request.toParams(), QN_STRINGIZE_TYPE(QnCameraBookmarkTagList), target, slot);
}
