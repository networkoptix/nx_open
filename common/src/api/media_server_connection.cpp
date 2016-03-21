#include "media_server_connection.h"

#include <cstring> /* For std::strstr. */

#include <QtCore/QSharedPointer>
#include <utils/common/uuid.h>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtGui/QImage>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkReply>

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
#include <nx_ec/data/api_camera_history_data.h>

#include <utils/common/util.h>
#include <utils/common/ldap.h>
#include <utils/common/warnings.h>
#include <utils/common/request_param.h>
#include <utils/common/model_functions.h>
#include <utils/network/http/httptypes.h>

#include <api/app_server_connection.h>
#include <event_log/events_serializer.h>

#include <recording/time_period_list.h>

#include "network_proxy_factory.h"
#include "session_manager.h"
#include "media_server_reply_processor.h"

#include "model/camera_list_reply.h"
#include "model/configure_reply.h"
#include "model/upload_update_reply.h"
#include "http/custom_headers.h"
#include "model/recording_stats_reply.h"
#include "common/common_module.h"

namespace {
    QN_DEFINE_LEXICAL_ENUM(RequestObject,
        (StorageStatusObject,      "storageStatus")
        (StorageSpaceObject,       "storageSpace")
        (TimePeriodsObject,        "RecordedTimePeriods")
        (StatisticsObject,         "statistics")
        (PtzContinuousMoveObject,  "ptz")
        (PtzContinuousFocusObject, "ptz")
        (PtzAbsoluteMoveObject,    "ptz")
        (PtzViewportMoveObject,    "ptz")
        (PtzGetPositionObject,     "ptz")
        (PtzCreatePresetObject,    "ptz")
        (PtzUpdatePresetObject,    "ptz")
        (PtzRemovePresetObject,    "ptz")
        (PtzActivatePresetObject,  "ptz")
        (PtzGetPresetsObject,      "ptz")
        (PtzCreateTourObject,      "ptz")
        (PtzRemoveTourObject,      "ptz")
        (PtzActivateTourObject,    "ptz")
        (PtzGetToursObject,        "ptz")
        (PtzGetHomeObjectObject,   "ptz")
        (PtzGetActiveObjectObject, "ptz")
        (PtzUpdateHomeObjectObject, "ptz")
        (PtzGetDataObject,         "ptz")
        (PtzGetAuxilaryTraitsObject, "ptz")
        (PtzRunAuxilaryCommandObject, "ptz")
        (GetParamsObject,          "getCameraParam")
        (SetParamsObject,          "setCameraParam")
        (TimeObject,               "gettime")
        (CameraSearchStartObject,  "manualCamera/search")
        (CameraSearchStatusObject, "manualCamera/status")
        (CameraSearchStopObject,   "manualCamera/stop")
        (CameraAddObject,          "manualCamera/add")
        (EventLogObject,           "events")
        (ImageObject,              "image")
        (checkCamerasObject,       "checkDiscovery")
        (CameraDiagnosticsObject,  "doCameraDiagnosticsStep")
        (GetSystemNameObject,      "getSystemName")
        (RebuildArchiveObject,     "rebuildArchive")
        (BackupControlObject,      "backupControl")
        (BookmarkAddObject,        "cameraBookmarks/add")
        (BookmarkUpdateObject,     "cameraBookmarks/update")
        (BookmarkDeleteObject,     "cameraBookmarks/delete")
        (InstallUpdateObject,      "installUpdate")
        (Restart,                  "restart")
        (ConfigureObject,          "configure")
        (PingSystemObject,         "pingSystem")
        (RecordingStatsObject,     "recStats")
        (AuditLogObject,           "auditLog")
        (MergeSystemsObject,       "mergeSystems")
        (TestEmailSettingsObject,  "testEmailSettings")
        (TestLdapSettingsObject,   "testLdapSettings")
        (ModulesInformationObject, "moduleInformation")
        (ec2CameraHistoryObject,   "ec2/cameraHistory")
        (ec2RecordedTimePeriodsObject, "ec2/recordedTimePeriods")
        (ec2BookmarksObject,       "ec2/bookmarks")
        (ec2BookmarkAddObject,     "ec2/bookmarks/add")
        (ec2BookmarkUpdateObject,  "ec2/bookmarks/update")
        (ec2BookmarkDeleteObject,  "ec2/bookmarks/delete")
        (ec2BookmarkTagsObject,    "ec2/bookmarks/tags")
        (MergeLdapUsersObject,     "mergeLdapUsers")
    );
#if 0
    QByteArray extractXmlBody(const QByteArray &body, const QByteArray &tagName, int *from = NULL)
    {
        QByteArray tagStart = QByteArray("<") + tagName + QByteArray(">");
        int bodyStart = body.indexOf(tagStart, from ? *from : 0);
        if (bodyStart >= 0)
            bodyStart += tagStart.length();
        QByteArray tagEnd = QByteArray("</") + tagName + QByteArray(">");
        int bodyEnd = body.indexOf(tagEnd, bodyStart);
        if (bodyStart >= 0 && bodyEnd >= 0){
            if (from)
                *from = bodyEnd + tagEnd.length();
            return body.mid(bodyStart, bodyEnd - bodyStart).trimmed();
        }
        else
            return QByteArray();
    }
#endif
} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnMediaServerReplyProcessor
// -------------------------------------------------------------------------- //
void QnMediaServerReplyProcessor::processReply(const QnHTTPRawResponse &response, int handle) {
    switch(object()) {
    case StorageStatusObject:
        processJsonReply<QnStorageStatusReply>(this, response, handle);
        break;
    case StorageSpaceObject:
        processJsonReply<QnStorageSpaceReply>(this, response, handle);
        break;
    case TimePeriodsObject: {
        int status = response.status;

        QnTimePeriodList reply;
        if(status == 0) {
            if (response.data.startsWith("BIN")) {
                reply.decode((const quint8*) response.data.constData() + 3, response.data.size() - 3);
            } else {
                qWarning() << "QnMediaServerConnection: unexpected message received.";
                status = -1;
            }
        }

        emitFinished(this, status, reply, handle);
        break;
    }
    case StatisticsObject: {
        processJsonReply<QnStatisticsReply>(this, response, handle);
        break;
    }
    case GetParamsObject:
	case SetParamsObject:
	{
		processJsonReply<QnCameraAdvancedParamValueList>(this, response, handle);
        break;
    }
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
    case PtzCreatePresetObject:
    case PtzUpdatePresetObject:
    case PtzRemovePresetObject:
    case PtzActivatePresetObject:
    case PtzCreateTourObject:
    case PtzRemoveTourObject:
    case PtzActivateTourObject:
    case PtzUpdateHomeObjectObject:
    case PtzRunAuxilaryCommandObject:
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

    case PtzGetAuxilaryTraitsObject:
        processJsonReply<QnPtzAuxilaryTraitList>(this, response, handle);
        break;

    case PtzGetDataObject:
        processJsonReply<QnPtzData>(this, response, handle);
        break;

    case CameraSearchStartObject:
    case CameraSearchStatusObject:
    case CameraSearchStopObject:
        processJsonReply<QnManualCameraSearchReply>(this, response, handle);
        break;
    case EventLogObject: {
        QnBusinessActionDataListPtr events(new QnBusinessActionDataList);
        if (response.status == 0)
            QnEventSerializer::deserialize(events, response.data);
        emitFinished(this, response.status, events, handle);
        break;
    }
    case ImageObject: {
        QImage image;
        if (response.status == 0)
            image.loadFromData(response.data);
        emitFinished(this, response.status, image, handle);
        break;
    }
    case checkCamerasObject:
        processJsonReply<QnCameraListReply>(this, response, handle);
        break;
    case CameraDiagnosticsObject:
        processJsonReply<QnCameraDiagnosticsReply>(this, response, handle);
        break;
    case GetSystemNameObject:
        emitFinished( this, response.status, QString::fromUtf8(response.data), handle );
        break;
    case RebuildArchiveObject: {
        processJsonReply<QnStorageScanData>(this, response, handle);
        break;
    }
    case BackupControlObject: {
        processJsonReply<QnBackupStatusData>(this, response, handle);
        break;
    }
    case ec2BookmarkAddObject:
    case ec2BookmarkUpdateObject:
    case ec2BookmarkDeleteObject:
        processJsonReply<QnCameraBookmark>(this, response, handle);
        break;
    case InstallUpdateObject:
        processJsonReply<QnUploadUpdateReply>(this, response, handle);
        break;
    case Restart:
        emitFinished(this, response.status, handle);
        break;
    case ConfigureObject:
        processJsonReply<QnConfigureReply>(this, response, handle);
        break;
    case ModulesInformationObject:
        processJsonReply<QList<QnModuleInformation>>(this, response, handle);
        break;
    case PingSystemObject:
        processJsonReply<QnModuleInformation>(this, response, handle);
        break;
    case RecordingStatsObject:
        processJsonReply<QnRecordingStatsReply>(this, response, handle);
        break;
    case AuditLogObject:
        processUbjsonReply<QnAuditRecordList>(this, response, handle);
        break;
    case MergeSystemsObject:
        processJsonReply<QnModuleInformation>(this, response, handle);
        break;
    case ec2CameraHistoryObject:
        processFusionReply<ec2::ApiCameraHistoryDataList>(this, response, handle);
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
        assert(false); /* We should never get here. */
        break;
    }

    deleteLater();
}


// -------------------------------------------------------------------------- //
// QnMediaServerConnection
// -------------------------------------------------------------------------- //
QnMediaServerConnection::QnMediaServerConnection(QnMediaServerResource* mserver, const QnUuid &videowallGuid, bool enableOfflineRequests, QObject *parent):
    base_type(parent, mserver),
    m_proxyPort(0),
    m_enableOfflineRequests(enableOfflineRequests)
{
    setUrl(mserver->getApiUrl());
    setSerializer(QnLexical::newEnumSerializer<RequestObject, int>());

    QnUuid guid = mserver->getOriginalGuid();

    QnRequestHeaderList queryParameters;
	queryParameters.insert(QString::fromLatin1(Qn::SERVER_GUID_HEADER_NAME), mserver->getId().toString());

    setExtraQueryParameters(queryParameters);

    QnRequestHeaderList extraHeaders;
	extraHeaders.insert(QString::fromLatin1(Qn::SERVER_GUID_HEADER_NAME), guid.isNull() ? mserver->getId().toString() : guid.toString());

    if (!videowallGuid.isNull())
        extraHeaders.insert(QString::fromLatin1(Qn::VIDEOWALL_GUID_HEADER_NAME), videowallGuid.toString());
    extraHeaders.insert(QString::fromLatin1(Qn::EC2_RUNTIME_GUID_HEADER_NAME), qnCommon->runningInstanceGUID().toString());
    extraHeaders.insert(QString::fromLatin1(Qn::CUSTOM_USERNAME_HEADER_NAME), QnAppServerConnectionFactory::url().userName());
	extraHeaders.insert(QString::fromLatin1("User-Agent"), QString::fromLatin1(nx_http::userAgentString()));
    setExtraHeaders(extraHeaders);
}

QnMediaServerConnection::~QnMediaServerConnection() {
    return;
}

QnAbstractReplyProcessor *QnMediaServerConnection::newReplyProcessor(int object) {
    return new QnMediaServerReplyProcessor(object);
}

bool QnMediaServerConnection::isReady() const {
    if (!targetResource())
        return false;

    if (m_enableOfflineRequests)
        return true;

    Qn::ResourceStatus status = targetResource()->getStatus();
    return status != Qn::Offline && status != Qn::NotDefined;
}

int QnMediaServerConnection::getThumbnailAsync(const QnNetworkResourcePtr &camera, qint64 timeUsec, const
                                                int rotation, const QSize& size, const QString& imageFormat, QnThumbnailRequestData::RoundMethod method, QObject *target, const char *slot)
{
    QnRequestParamList params;

    params << QnRequestParam("res_id", camera->getPhysicalId());
    if (timeUsec < 0)
        params << QnRequestParam("time", "latest");
    else
        params << QnRequestParam("time", timeUsec);
    if (size.width() > 0)
        params << QnRequestParam("width", size.width());
    if (size.height() > 0)
        params << QnRequestParam("height", size.height());
    if (rotation != -1)
        params << QnRequestParam("rotate", rotation);
    params << QnRequestParam("format", imageFormat);
    params << QnRequestParam("method", QnLexical::serialized(method));
    return sendAsyncGetRequest(ImageObject, params, QN_STRINGIZE_TYPE(QImage), target, slot);
}

int QnMediaServerConnection::checkCameraList(const QnNetworkResourceList &cameras, QObject *target, const char *slot)
{
    QnCameraListReply camList;
    for(const QnResourcePtr& c: cameras)
        camList.uniqueIdList << c->getUniqueId();

    QnRequestHeaderList headers;
    headers << QnRequestParam("content-type",   "application/json");

    return sendAsyncPostRequest(checkCamerasObject, headers, QnRequestParamList(), QJson::serialized(camList), QN_STRINGIZE_TYPE(QnCameraListReply), target, slot);
}

int QnMediaServerConnection::getTimePeriodsAsync(const QnVirtualCameraResourcePtr &camera,
                                                 qint64 startTimeMs,
                                                 qint64 endTimeMs,
                                                 qint64 detail,
                                                 Qn::TimePeriodContent periodsType,
                                                 const QString &filter,
                                                 QObject *target,
                                                 const char *slot)
{
    QnRequestParamList params;

    params << QnRequestParam(QLatin1String(Qn::CAMERA_UNIQUE_ID_HEADER_NAME), camera->getPhysicalId());
    params << QnRequestParam("startTime", QString::number(startTimeMs));
    params << QnRequestParam("endTime", QString::number(endTimeMs));
    params << QnRequestParam("detail", QString::number(detail));
    params << QnRequestParam("format", "bin");
    params << QnRequestParam("periodsType", QString::number(static_cast<int>(periodsType)));
    params << QnRequestParam("filter", filter);

#ifdef QN_MEDIA_SERVER_API_DEBUG
    QString path = url().toDisplayString() + lit("/api/RecordedTimePeriods?t=1");
    for (const QnRequestParam &param: params)
        path += L'&' + param.first + L'=' + param.second;
    qDebug() << "Requesting chunks" << path;
#endif

    return sendAsyncGetRequest(TimePeriodsObject, params, QN_STRINGIZE_TYPE(QnTimePeriodList), target, slot);
}

int QnMediaServerConnection::getParamsAsync(const QnNetworkResourcePtr &camera, const QStringList &keys, QObject *target, const char *slot) {
	Q_ASSERT_X(!keys.isEmpty(), Q_FUNC_INFO, "parameter names should be provided");

	QnRequestParamList params;
    params << QnRequestParam("res_id", camera->getPhysicalId());
    for(const QString &param: keys)
        params << QnRequestParam(param, QString());

    return sendAsyncGetRequest(GetParamsObject, params, QN_STRINGIZE_TYPE(QnCameraAdvancedParamValueList), target, slot);
}

int QnMediaServerConnection::setParamsAsync(const QnNetworkResourcePtr &camera, const QnCameraAdvancedParamValueList &values, QObject *target, const char *slot) {
	Q_ASSERT_X(!values.isEmpty(), Q_FUNC_INFO, "parameter names should be provided");

    QnRequestParamList params;
    params << QnRequestParam("res_id", camera->getPhysicalId());
    for(const QnCameraAdvancedParamValue value: values)
        params << QnRequestParam(value.id, value.value);

    return sendAsyncGetRequest(SetParamsObject, params, QN_STRINGIZE_TYPE(QnCameraAdvancedParamValueList), target, slot);
}

int QnMediaServerConnection::searchCameraAsyncStart(const QString &startAddr, const QString &endAddr, const QString &username, const QString &password, int port, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("start_ip", startAddr);
    if (!endAddr.isEmpty())
        params << QnRequestParam("end_ip", endAddr);
    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);
    params << QnRequestParam("port" ,QString::number(port));

    return sendAsyncGetRequest(CameraSearchStartObject, params, QN_STRINGIZE_TYPE(QnManualCameraSearchReply), target, slot);
}

int QnMediaServerConnection::searchCameraAsyncStatus(const QnUuid &processUuid, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("uuid", processUuid.toString());
    return sendAsyncGetRequest(CameraSearchStatusObject, params, QN_STRINGIZE_TYPE(QnManualCameraSearchReply), target, slot);
}

int QnMediaServerConnection::searchCameraAsyncStop(const QnUuid &processUuid, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("uuid", processUuid.toString());
    return sendAsyncGetRequest(CameraSearchStopObject, params, QN_STRINGIZE_TYPE(QnManualCameraSearchReply), target, slot);
}

int QnMediaServerConnection::addCameraAsync(const QnManualCameraSearchCameraList& cameras, const QString &username, const QString &password, QObject *target, const char *slot) {
    QnRequestParamList params;
    for (int i = 0; i < cameras.size(); i++){
        params << QnRequestParam(lit("url") + QString::number(i), cameras[i].url);
        params << QnRequestParam(lit("manufacturer") + QString::number(i), cameras[i].manufacturer);
        params << QnRequestParam(lit("uniqueId") + QString::number(i), cameras[i].uniqueId);
    }
    params << QnRequestParam("user", username);
    params << QnRequestParam("password", password);

    return sendAsyncGetRequest(CameraAddObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzContinuousMoveAsync(const QnNetworkResourcePtr &camera, const QVector3D &speed, const QnUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::ContinuousMovePtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("xSpeed",          QnLexical::serialized(speed.x()));
    params << QnRequestParam("ySpeed",          QnLexical::serialized(speed.y()));
    params << QnRequestParam("zSpeed",          QnLexical::serialized(speed.z()));
    params << QnRequestParam("sequenceId",      QnLexical::serialized(sequenceId));
    params << QnRequestParam("sequenceNumber",  QnLexical::serialized(sequenceNumber));

    return sendAsyncGetRequest(PtzContinuousMoveObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzContinuousFocusAsync(const QnNetworkResourcePtr &camera, qreal speed, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::ContinuousFocusPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("speed",           QnLexical::serialized(speed));

    return sendAsyncGetRequest(PtzContinuousFocusObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzAbsoluteMoveAsync(const QnNetworkResourcePtr &camera, Qn::PtzCoordinateSpace space, const QVector3D &position, qreal speed, const QnUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(space == Qn::DevicePtzCoordinateSpace ? Qn::AbsoluteDeviceMovePtzCommand : Qn::AbsoluteLogicalMovePtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("xPos",            QnLexical::serialized(position.x()));
    params << QnRequestParam("yPos",            QnLexical::serialized(position.y()));
    params << QnRequestParam("zPos",            QnLexical::serialized(position.z()));
    params << QnRequestParam("speed",           QnLexical::serialized(speed));
    params << QnRequestParam("sequenceId",      QnLexical::serialized(sequenceId));
    params << QnRequestParam("sequenceNumber",  QnLexical::serialized(sequenceNumber));

    return sendAsyncGetRequest(PtzAbsoluteMoveObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzViewportMoveAsync(const QnNetworkResourcePtr &camera, qreal aspectRatio, const QRectF &viewport, qreal speed, const QnUuid &sequenceId, int sequenceNumber, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::ViewportMovePtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("aspectRatio",     QnLexical::serialized(aspectRatio));
    params << QnRequestParam("viewportTop",     QnLexical::serialized(viewport.top()));
    params << QnRequestParam("viewportLeft",    QnLexical::serialized(viewport.left()));
    params << QnRequestParam("viewportBottom",  QnLexical::serialized(viewport.bottom()));
    params << QnRequestParam("viewportRight",   QnLexical::serialized(viewport.right()));
    params << QnRequestParam("speed",           QnLexical::serialized(speed));
    params << QnRequestParam("sequenceId",      QnLexical::serialized(sequenceId));
    params << QnRequestParam("sequenceNumber",  QnLexical::serialized(sequenceNumber));

    return sendAsyncGetRequest(PtzViewportMoveObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzGetPositionAsync(const QnNetworkResourcePtr &camera, Qn::PtzCoordinateSpace space, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(space == Qn::DevicePtzCoordinateSpace ? Qn::GetDevicePositionPtzCommand : Qn::GetLogicalPositionPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetPositionObject, params, QN_STRINGIZE_TYPE(QVector3D), target, slot);
}

int QnMediaServerConnection::ptzCreatePresetAsync(const QnNetworkResourcePtr &camera, const QnPtzPreset &preset, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::CreatePresetPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("presetName",      QnLexical::serialized(preset.name));
    params << QnRequestParam("presetId",        QnLexical::serialized(preset.id));

    return sendAsyncGetRequest(PtzCreatePresetObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzUpdatePresetAsync(const QnNetworkResourcePtr &camera, const QnPtzPreset &preset, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::UpdatePresetPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("presetName",      QnLexical::serialized(preset.name));
    params << QnRequestParam("presetId",        QnLexical::serialized(preset.id));

    return sendAsyncGetRequest(PtzUpdatePresetObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzRemovePresetAsync(const QnNetworkResourcePtr &camera, const QString &presetId, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::RemovePresetPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("presetId",        QnLexical::serialized(presetId));

    return sendAsyncGetRequest(PtzRemovePresetObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzActivatePresetAsync(const QnNetworkResourcePtr &camera, const QString &presetId, qreal speed, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::ActivatePresetPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("presetId",        QnLexical::serialized(presetId));
    params << QnRequestParam("speed",           QnLexical::serialized(speed));

    return sendAsyncGetRequest(PtzActivatePresetObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzGetPresetsAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetPresetsPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetPresetsObject, params, QN_STRINGIZE_TYPE(QnPtzPresetList), target, slot);
}

int QnMediaServerConnection::ptzCreateTourAsync(const QnNetworkResourcePtr &camera, const QnPtzTour &tour, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",          QnLexical::serialized(Qn::CreateTourPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    QnRequestHeaderList headers;
    headers << QnRequestParam("content-type",   "application/json");

    return sendAsyncPostRequest(PtzCreateTourObject, headers, params, QJson::serialized(tour), NULL, target, slot);
}

int QnMediaServerConnection::ptzRemoveTourAsync(const QnNetworkResourcePtr &camera, const QString &tourId, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::RemoveTourPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("tourId",          QnLexical::serialized(tourId));

    return sendAsyncGetRequest(PtzRemoveTourObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzActivateTourAsync(const QnNetworkResourcePtr &camera, const QString &tourId, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::ActivateTourPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("tourId",          QnLexical::serialized(tourId));

    return sendAsyncGetRequest(PtzActivateTourObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzGetToursAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetToursPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetToursObject, params, QN_STRINGIZE_TYPE(QnPtzTourList), target, slot);
}

int QnMediaServerConnection::ptzGetActiveObjectAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetActiveObjectPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetActiveObjectObject, params, QN_STRINGIZE_TYPE(QnPtzObject), target, slot);
}

int QnMediaServerConnection::ptzUpdateHomeObjectAsync(const QnNetworkResourcePtr &camera, const QnPtzObject &homePosition, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::UpdateHomeObjectPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("objectType",      QnLexical::serialized(homePosition.type));
    params << QnRequestParam("objectId",        QnLexical::serialized(homePosition.id));

    return sendAsyncGetRequest(PtzUpdateHomeObjectObject, params, QN_STRINGIZE_TYPE(QnPtzObject), target, slot);
}

int QnMediaServerConnection::ptzGetHomeObjectAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetHomeObjectPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetHomeObjectObject, params, QN_STRINGIZE_TYPE(QnPtzObject), target, slot);
}

int QnMediaServerConnection::ptzGetAuxilaryTraitsAsync(const QnNetworkResourcePtr &camera, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetAuxilaryTraitsPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));

    return sendAsyncGetRequest(PtzGetAuxilaryTraitsObject, params, QN_STRINGIZE_TYPE(QnPtzAuxilaryTraitList), target, slot);
}

int QnMediaServerConnection::ptzRunAuxilaryCommandAsync(const QnNetworkResourcePtr &camera, const QnPtzAuxilaryTrait &trait, const QString &data, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::RunAuxilaryCommandPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("trait",           QnLexical::serialized(trait));
    params << QnRequestParam("data",            QnLexical::serialized(data));

    return sendAsyncGetRequest(PtzRunAuxilaryCommandObject, params, NULL, target, slot);
}

int QnMediaServerConnection::ptzGetDataAsync(const QnNetworkResourcePtr &camera, Qn::PtzDataFields query, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("command",         QnLexical::serialized(Qn::GetDataPtzCommand));
    params << QnRequestParam("resourceId",      QnLexical::serialized(camera->getPhysicalId()));
    params << QnRequestParam("query",           QnLexical::serialized(query));

    return sendAsyncGetRequest(PtzGetDataObject, params, QN_STRINGIZE_TYPE(QnPtzData), target, slot);
}

int QnMediaServerConnection::getTimeAsync(QObject *target, const char *slot) {
    return sendAsyncGetRequest(TimeObject, QnRequestParamList(), QN_STRINGIZE_TYPE(QnTimeReply), target, slot);
}

int QnMediaServerConnection::mergeLdapUsersAsync(QObject *target, const char *slot) {
    return sendAsyncGetRequest(MergeLdapUsersObject, QnRequestParamList(), nullptr, target, slot);
}

int QnMediaServerConnection::getSystemNameAsync( QObject* target, const char* slot )
{
    return sendAsyncGetRequest(GetSystemNameObject, QnRequestParamList(), QN_STRINGIZE_TYPE(QString), target, slot);
}

int QnMediaServerConnection::testEmailSettingsAsync(const QnEmailSettings &settings, QObject *target, const char *slot)
{
    QnRequestHeaderList headers;
    headers << QnRequestParam("content-type",   "application/json");
    ec2::ApiEmailSettingsData data;
    ec2::fromResourceToApi(settings, data);
    return sendAsyncPostRequest(TestEmailSettingsObject, headers, QnRequestParamList(), QJson::serialized(data), QN_STRINGIZE_TYPE(QnTestEmailSettingsReply), target, slot);
}

int QnMediaServerConnection::testLdapSettingsAsync(const QnLdapSettings &settings, QObject *target, const char *slot)
{
    QnRequestHeaderList headers;
    headers << QnRequestParam("content-type",   "application/json");
    return sendAsyncPostRequest(TestLdapSettingsObject, headers, QnRequestParamList(), QJson::serialized(settings), QN_STRINGIZE_TYPE(QnLdapUsers), target, slot);
}

int QnMediaServerConnection::doCameraDiagnosticsStepAsync(
    const QnUuid& cameraID, CameraDiagnostics::Step::Value previousStep,
    QObject* target, const char* slot )
{
    QnRequestParamList params;
    params << QnRequestParam("res_id",  cameraID);
    params << QnRequestParam("type", CameraDiagnostics::Step::toString(previousStep));
    return sendAsyncGetRequest(CameraDiagnosticsObject, params, QN_STRINGIZE_TYPE(QnCameraDiagnosticsReply), target, slot);
}

int QnMediaServerConnection::doRebuildArchiveAsync(Qn::RebuildAction action, bool isMainPool, QObject *target, const char *slot)
{
    QnRequestParamList params;
    params << QnRequestParam("action",  QnLexical::serialized(action));
    params << QnRequestParam("mainPool", isMainPool);
    return sendAsyncGetRequest(RebuildArchiveObject, params, QN_STRINGIZE_TYPE(QnStorageScanData), target, slot);
}

int QnMediaServerConnection::backupControlActionAsync(Qn::BackupAction action, QObject *target, const char *slot)
{
    QnRequestParamList params;
    params << QnRequestParam("action",  QnLexical::serialized(action));
    return sendAsyncGetRequest(BackupControlObject, params, QN_STRINGIZE_TYPE(QnBackupStatusData), target, slot);
}

int QnMediaServerConnection::getStorageSpaceAsync(bool fastRequest, QObject *target, const char *slot)
{
    QnRequestParamList params;
    if (fastRequest)
        params << QnRequestParam("fast", QnLexical::serialized(true));
    return sendAsyncGetRequest(StorageSpaceObject, params, QN_STRINGIZE_TYPE(QnStorageSpaceReply), target, slot);
}

int QnMediaServerConnection::getStorageStatusAsync(const QString &storageUrl, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("path", storageUrl);

    return sendAsyncGetRequest(StorageStatusObject, params, QN_STRINGIZE_TYPE(QnStorageStatusReply), target, slot);
}

int QnMediaServerConnection::getStatisticsAsync(QObject *target, const char *slot){
    return sendAsyncGetRequest(StatisticsObject, QnRequestParamList(), QN_STRINGIZE_TYPE(QnStatisticsReply), target, slot);
}

int QnMediaServerConnection::getEventLogAsync(
                  qint64 dateFrom, qint64 dateTo,
                  QnResourceList camList,
                  QnBusiness::EventType eventType,
                  QnBusiness::ActionType actionType,
                  QnUuid businessRuleId,
                  QObject *target, const char *slot)
{
    QnRequestParamList params;
    params << QnRequestParam( "from", dateFrom);
    if (dateTo != DATETIME_NOW)
        params << QnRequestParam( "to", dateTo);
    for(const QnResourcePtr& res: camList) {
        QnNetworkResourcePtr camera = res.dynamicCast<QnNetworkResource>();
        if (camera)
            params << QnRequestParam( "res_id", camera->getPhysicalId() );
    }
    if (!businessRuleId.isNull())
        params << QnRequestParam( "brule_id", businessRuleId );
    if (eventType != QnBusiness::UndefinedEvent)
        params << QnRequestParam( "event", (int) eventType);
    if (actionType != QnBusiness::UndefinedAction)
        params << QnRequestParam( "action", (int) actionType);

    return sendAsyncGetRequest(EventLogObject, params, QN_STRINGIZE_TYPE(QnBusinessActionDataListPtr), target, slot);
}

int QnMediaServerConnection::installUpdate(const QString &updateId, bool delayed, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("updateId", updateId);
    params << QnRequestParam("delayed", delayed);

    return sendAsyncGetRequest(InstallUpdateObject, params, QN_STRINGIZE_TYPE(QnUploadUpdateReply), target, slot);
}

int QnMediaServerConnection::uploadUpdateChunk(const QString &updateId, const QByteArray &data, qint64 offset, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("updateId", updateId);
    params << QnRequestParam("offset", offset);

    QnRequestHeaderList headers;
    headers << QnRequestParam("content-type",   "text/xml");

    return sendAsyncPostRequest(InstallUpdateObject, headers, params, data, QN_STRINGIZE_TYPE(QnUploadUpdateReply), target, slot);
}

int QnMediaServerConnection::restart(QObject *target, const char *slot) {
    return sendAsyncGetRequest(Restart, QnRequestParamList(), NULL, target, slot);
}

int QnMediaServerConnection::configureAsync(bool wholeSystem, const QString &systemName, const QString &password, const QByteArray &passwordHash,
    const QByteArray &passwordDigest, const QByteArray &cryptSha512Hash, int port, QObject *target, const char *slot)
{
    QnRequestParamList params;
    params << QnRequestParam("wholeSystem", wholeSystem ? lit("true") : lit("false"));
    params << QnRequestParam("systemName", systemName);
    params << QnRequestParam("password", password);
    params << QnRequestParam("passwordHash", QString::fromLatin1(passwordHash));
    params << QnRequestParam("passwordDigest", QString::fromLatin1(passwordDigest));
    params << QnRequestParam("cryptSha512Hash", QString::fromLatin1(cryptSha512Hash) );
    params << QnRequestParam("port", port);

    return sendAsyncGetRequest(ConfigureObject, params, QN_STRINGIZE_TYPE(QnConfigureReply), target, slot);
}

int QnMediaServerConnection::pingSystemAsync(const QUrl &url, const QString &password, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("url", url.toString());
    params << QnRequestParam("password", password);

    return sendAsyncGetRequest(PingSystemObject, params, QN_STRINGIZE_TYPE(QnModuleInformation), target, slot);
}

int QnMediaServerConnection::getRecordingStatisticsAsync(qint64 bitrateAnalizePeriodMs, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("bitrateAnalizePeriodMs", bitrateAnalizePeriodMs);
    return sendAsyncGetRequest(RecordingStatsObject, params, QN_STRINGIZE_TYPE(QnRecordingStatsReply), target, slot);
}

int QnMediaServerConnection::getAuditLogAsync(qint64 startTimeMs, qint64 endTimeMs, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("from", startTimeMs * 1000ll);
    params << QnRequestParam("to", endTimeMs * 1000ll);
    params << QnRequestParam("format", "ubjson");
    return sendAsyncGetRequest(AuditLogObject, params, QN_STRINGIZE_TYPE(QnAuditRecordList), target, slot);
}

int QnMediaServerConnection::mergeSystemAsync(const QUrl &url, const QString &password, const QString &currentPassword, bool ownSettings, bool oneServer, bool ignoreIncompatible, QObject *target, const char *slot) {
    QnRequestParamList params;
    params << QnRequestParam("url", url.toString());
    params << QnRequestParam("password", password);
    params << QnRequestParam("currentPassword", currentPassword);
    params << QnRequestParam("takeRemoteSettings", !ownSettings ? lit("true") : lit("false"));
    params << QnRequestParam("oneServer", oneServer ? lit("true") : lit("false"));
    params << QnRequestParam("ignoreIncompatible", ignoreIncompatible ? lit("true") : lit("false"));

    return sendAsyncGetRequest(MergeSystemsObject, params, QN_STRINGIZE_TYPE(QnModuleInformation), target, slot);
}

int QnMediaServerConnection::modulesInformation(QObject *target, const char *slot)
{
    QnRequestParamList params;
    params << QnRequestParam("allModules", lit("true"));
    return sendAsyncGetRequest(ModulesInformationObject, params, QN_STRINGIZE_TYPE(QList<QnModuleInformation>), target, slot);
}

int QnMediaServerConnection::cameraHistory(const QnChunksRequestData &request, QObject *target, const char *slot) {
    return sendAsyncGetRequest(ec2CameraHistoryObject, request.toParams(), QN_STRINGIZE_TYPE(ec2::ApiCameraHistoryDataList) ,target, slot);
}

int QnMediaServerConnection::recordedTimePeriods(const QnChunksRequestData &request, QObject *target, const char *slot) {
    QnChunksRequestData fixedFormatRequest(request);
    fixedFormatRequest.format = Qn::CompressedPeriodsFormat;
    return sendAsyncGetRequest(ec2RecordedTimePeriodsObject, fixedFormatRequest.toParams(), QN_STRINGIZE_TYPE(MultiServerPeriodDataList) ,target, slot);
}

int QnMediaServerConnection::addBookmarkAsync(const QnCameraBookmark &bookmark, QObject *target, const char *slot) {
    QnUpdateBookmarkRequestData request(bookmark);
    return sendAsyncGetRequest(ec2BookmarkAddObject, request.toParams(), nullptr ,target, slot);
}

int QnMediaServerConnection::updateBookmarkAsync(const QnCameraBookmark &bookmark, QObject *target, const char *slot) {
    QnUpdateBookmarkRequestData request(bookmark);
    return sendAsyncGetRequest(ec2BookmarkUpdateObject, request.toParams(), nullptr ,target, slot);
}

int QnMediaServerConnection::deleteBookmarkAsync(const QnUuid &bookmarkId, QObject *target, const char *slot) {
    QnDeleteBookmarkRequestData request(bookmarkId);
    return sendAsyncGetRequest(ec2BookmarkDeleteObject, request.toParams(), nullptr ,target, slot);
}

int QnMediaServerConnection::getBookmarksAsync(const QnGetBookmarksRequestData &request, QObject *target, const char *slot) {
    return sendAsyncGetRequest(ec2BookmarksObject, request.toParams(), QN_STRINGIZE_TYPE(QnCameraBookmarkList) ,target, slot);
}

int QnMediaServerConnection::getBookmarkTagsAsync(const QnGetBookmarkTagsRequestData &request, QObject *target, const char *slot) {
    return sendAsyncGetRequest(ec2BookmarkTagsObject, request.toParams(), QN_STRINGIZE_TYPE(QnCameraBookmarkTagList), target, slot);

}
