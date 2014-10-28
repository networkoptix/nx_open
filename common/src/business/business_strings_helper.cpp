#include "business_strings_helper.h"
#include <utils/common/app_info.h>

#include <api/app_server_connection.h>

#include "utils/common/id.h"
#include "utils/network/nettools.h" /* For resolveAddress. */

#include <business/business_aggregation_info.h>
#include <business/events/reasoned_business_event.h>
#include <business/events/network_issue_business_event.h>
#include <business/events/camera_input_business_event.h>
#include <business/events/conflict_business_event.h>
#include <business/events/mserver_conflict_business_event.h>

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>


namespace {
    static const QString plainTextDelimiter(lit("\n"));
    static const QString htmlDelimiter(lit("<br>"));

    static const QString tpProductName(lit("productName"));
    static const QString tpEvent(lit("event"));
    static const QString tpSource(lit("source"));
    static const QString tpUrlInt(lit("urlint"));
    static const QString tpUrlExt(lit("urlext"));
    static const QString tpTimestamp(lit("timestamp"));
    static const QString tpReason(lit("reason"));
    static const QString tpAggregated(lit("aggregated"));
    static const QString tpInputPort(lit("inputPort"));
}

QString QnBusinessStringsHelper::actionName(QnBusiness::ActionType value) {
    using namespace QnBusiness;

    /* Do not use 'default' keyword! 
     * Warning should be raised on unknown enumeration values. */
    switch(value) {
    case UndefinedAction:           return QString();
    case CameraOutputAction:        return tr("Camera output");
    case CameraOutputOnceAction:    return tr("Camera output for 30 sec");
    case BookmarkAction:            return tr("Bookmark");
    case CameraRecordingAction:     return tr("Camera recording");
    case PanicRecordingAction:      return tr("Panic recording");
    case SendMailAction:            return tr("Send email");
    case DiagnosticsAction:         return tr("Write to log");
    case ShowPopupAction:           return tr("Show notification");
    case PlaySoundAction:           return tr("Repeat sound");
    case PlaySoundOnceAction:       return tr("Play sound");
    case SayTextAction:             return tr("Speak");
    default:                        return tr("Unknown (%1)").arg(static_cast<int>(value));
    }
}

QString QnBusinessStringsHelper::eventName(QnBusiness::EventType value) {
    using namespace QnBusiness;

    if (value >= UserEvent)
        return tr("User Defined (%1)").arg((int)value - (int)UserEvent);

    switch( value )
    {
    case CameraMotionEvent:     return tr("Motion on Camera");
    case CameraInputEvent:      return tr("Input Signal on Camera");
    case CameraDisconnectEvent: return tr("Camera Disconnected");
    case StorageFailureEvent:   return tr("Storage Failure");
    case NetworkIssueEvent:     return tr("Network Issue");
    case CameraIpConflictEvent: return tr("Camera IP Conflict");
    case ServerFailureEvent:    return tr("Server Failure");
    case ServerConflictEvent:   return tr("Server Conflict");
    case ServerStartEvent:      return tr("Server Started");
    case LicenseIssueEvent:     return tr("License Issue");
    case AnyCameraEvent:        return tr("Any Camera Issue");
    case AnyServerEvent:        return tr("Any Server Issue");
    case AnyBusinessEvent:      return tr("Any Event");
    default:
        return QString();
    }
}

QString QnBusinessStringsHelper::eventAtResource(const QnBusinessEventParameters &params, bool useIp) {
    using namespace QnBusiness;

    QString resourceName = eventSource(params, useIp);
    switch (params.eventType) {
    case UndefinedEvent:
        return tr("Undefined event has occurred on %1").arg(resourceName);

    case CameraDisconnectEvent:
        return tr("Camera %1 was disconnected").arg(resourceName);

    case CameraInputEvent:
        return tr("Input on %1").arg(resourceName);

    case CameraMotionEvent:
        return tr("Motion on %1").arg(resourceName);

    case StorageFailureEvent:
        return tr("Storage Failure at %1").arg(resourceName);

    case NetworkIssueEvent:
        return tr("Network Issue at %1").arg(resourceName);

    case ServerFailureEvent:
        return tr("Server \"%1\" Failure").arg(resourceName);

    case CameraIpConflictEvent:
        return tr("Camera IP Conflict at %1").arg(resourceName);

    case ServerConflictEvent:
        return tr("Server \"%1\" Conflict").arg(resourceName);

    case ServerStartEvent:
        return tr("Server \"%1\" Started").arg(resourceName);
    case LicenseIssueEvent:
        return tr("Server \"%1\" had license issue").arg(resourceName);

    default:
        break;
    }
    return tr("Unknown event has occurred");
}

QString QnBusinessStringsHelper::eventAtResources(const QnBusinessEventParameters &params, int /*resourceCount*/)
{
    return lit("Multiple %1 events have occured").arg(eventName(params.eventType));
}

QString QnBusinessStringsHelper::eventDescription(const QnAbstractBusinessActionPtr& action, const QnBusinessAggregationInfo &aggregationInfo, bool useIp, bool useHtml) {

    QString delimiter = useHtml
            ? htmlDelimiter
            : plainTextDelimiter;

    QnBusinessEventParameters params = action->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    QString result;
    result += tr("Event: %1").arg(eventName(eventType));

    result += delimiter;
    result += tr("Source: %1").arg(eventSource(params, useIp));

    if (useHtml && eventType == QnBusiness::CameraMotionEvent) {
        result += delimiter;
        result += tr("Url: %1").arg(motionUrl(params, true));
    }

    result += delimiter;
    result += aggregatedEventDetails(action, aggregationInfo, delimiter);

    return result;
}

QVariantHash QnBusinessStringsHelper::eventDescriptionMap(const QnAbstractBusinessActionPtr& action, const QnBusinessAggregationInfo &aggregationInfo, bool useIp) {
    QnBusinessEventParameters params = action->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    QVariantHash contextMap;

    contextMap[tpProductName] = QnAppInfo::productNameLong();
    contextMap[tpEvent] = eventName(eventType);
    contextMap[tpSource] = eventSource(params, useIp);
    if (eventType == QnBusiness::CameraMotionEvent) {
        contextMap[tpUrlInt] = motionUrl(params, false);
        contextMap[tpUrlExt] = motionUrl(params, true);
    }
    contextMap[tpAggregated] = aggregatedEventDetailsMap(action, aggregationInfo, useIp);

    return contextMap;
}

QString QnBusinessStringsHelper::eventDetailsWithTimestamp(const QnBusinessEventParameters &params, int aggregationCount, const QString& delimiter) {
    return eventTimestamp(params, aggregationCount) + delimiter + eventDetails(params, aggregationCount, delimiter);
}

QString QnBusinessStringsHelper::eventDetails(const QnBusinessEventParameters &params, int aggregationCount, const QString& delimiter) {
    using namespace QnBusiness;

    Q_UNUSED(aggregationCount)
    QString result;

    switch (params.eventType) {
    case CameraInputEvent: {
        result = tr("Input port: %1").arg(params.inputPortId);
        break;
    }
    case StorageFailureEvent:
    case NetworkIssueEvent:
    case ServerFailureEvent: 
    case LicenseIssueEvent:
    {
        result += tr("Reason: %1").arg(eventReason(params));
        break;
    }
    case CameraIpConflictEvent: {
        result += tr("Conflict address: %1").arg(params.source);

        int n = 0;
        for (const QString& mac: params.conflicts) {
            result += delimiter;
            result += tr("Camera #%1 MAC: %2").arg(++n).arg(mac);
        }
        break;
    }
    case ServerConflictEvent: {
        QnCameraConflictList conflicts;
        conflicts.sourceServer = params.source;
        conflicts.decode(params.conflicts);
        int n = 0;
        for (auto itr = conflicts.camerasByServer.begin(); itr != conflicts.camerasByServer.end(); ++itr) {
            const QString &server = itr.key();
            result += delimiter;
            result += tr("Conflicting Server #%1: %2").arg(++n).arg(server);
            int m = 0;
            for (const QString &camera: conflicts.camerasByServer[server]) {
                result += delimiter;
                result += tr("Camera #%1 MAC: %2").arg(++m).arg(camera);
            }

        }
        break;
    }
    case ServerStartEvent: 
        break;
    default:
        break;
    }
    return result;
}

QVariantHash QnBusinessStringsHelper::eventDetailsMap(
    const QnAbstractBusinessActionPtr& action,
    const QnInfoDetail& aggregationData,
    bool useIp,
    bool addSubAggregationData )
{
    using namespace QnBusiness;

    const QnBusinessEventParameters& params = aggregationData.runtimeParams();
    const int aggregationCount = aggregationData.count();

    QVariantHash detailsMap;

    if( addSubAggregationData )
    {
        const QnBusinessAggregationInfo& subAggregationData = aggregationData.subAggregationData();
        detailsMap[tpAggregated] = !subAggregationData.isEmpty()
            ? aggregatedEventDetailsMap(action, subAggregationData, useIp)
            : (QVariantList() << eventDetailsMap(action, aggregationData, useIp, false));
    }

    detailsMap[tpTimestamp] = eventTimestampShort(params, aggregationCount);

    switch (params.eventType) {
    case CameraDisconnectEvent: {
        detailsMap[tpSource] = eventSource(params, useIp);
        break;
    }

    case CameraInputEvent: {
        detailsMap[tpInputPort] = params.inputPortId;
        break;
    }
    case StorageFailureEvent:
    case NetworkIssueEvent:
    case ServerFailureEvent: 
    case LicenseIssueEvent:
    {
        detailsMap[tpReason] = eventReason(params);
        break;
    }
    case CameraIpConflictEvent: {
        detailsMap[lit("cameraConflictAddress")] = params.source;
        QVariantList conflicts;
        int n = 0;
        foreach (const QString& mac, params.conflicts) {
            QVariantHash conflict;
            conflict[lit("number")] = ++n;
            conflict[lit("mac")] = mac;
            conflicts << conflict;
        }
        detailsMap[lit("cameraConflicts")] = conflicts;

        break;
    }
    case ServerConflictEvent: {
        QnCameraConflictList conflicts;
        conflicts.sourceServer = params.source;
        conflicts.decode(params.conflicts);

        QVariantList conflictsList;
        int n = 0;
        for (auto itr = conflicts.camerasByServer.begin(); itr != conflicts.camerasByServer.end(); ++itr) {
            const QString &server = itr.key();
            foreach (const QString &camera, conflicts.camerasByServer[server]) {
                QVariantHash conflict;
                conflict[lit("number")] = ++n;
                conflict[lit("ip")] = server;
                conflict[lit("mac")] = camera;
                conflictsList << conflict;
            }
        }
        detailsMap[lit("msConflicts")] = conflictsList;
        break;
    }
    default:
        break;
    }
    return detailsMap;
}


QString QnBusinessStringsHelper::eventTimestampShort(const QnBusinessEventParameters &params, int aggregationCount) {
    quint64 ts = params.eventTimestamp;
    QDateTime time = QDateTime::fromMSecsSinceEpoch(ts/1000);

    int count = qMax(aggregationCount, 1);
    if (count == 1)
        return tr("%2 %1", "%1 means time, %2 means date")
            .arg(time.time().toString())
            .arg(time.date().toString());
    else
        return tr("%n times, first: %2 %1", "%1 means time, %2 means date", count)
            .arg(time.time().toString())
            .arg(time.date().toString());
}

QString QnBusinessStringsHelper::eventTimestamp(const QnBusinessEventParameters &params, int aggregationCount) {
    quint64 ts = params.eventTimestamp;
    QDateTime time = QDateTime::fromMSecsSinceEpoch(ts/1000);

    int count = qMax(aggregationCount, 1);
    if (count == 1)
        return tr("Time: %1 on %2", "%1 means time, %2 means date")
            .arg(time.time().toString())
            .arg(time.date().toString());
    else
        return tr("First occurrence: %1 on %2 (%n times total)", "%1 means time, %2 means date", count)
            .arg(time.time().toString())
            .arg(time.date().toString());
}

QString QnBusinessStringsHelper::eventSource(const QnBusinessEventParameters &params, bool useIp) {
    QnUuid id = params.eventResourceId;
    QnResourcePtr res = !id.isNull() ? qnResPool->getResourceById(id) : QnResourcePtr();
    return getFullResourceName(res, useIp);
}

QString QnBusinessStringsHelper::eventReason(const QnBusinessEventParameters& params) {
    using namespace QnBusiness;
    
    QString reasonParamsEncoded = params.reasonParamsEncoded;

    QString result;
    switch (params.reasonCode) {
    case NetworkNoFrameReason: {
        int msecs = QnNetworkIssueBusinessEvent::decodeTimeoutMsecs(reasonParamsEncoded, 5000);
        result = tr("No video frame received during last %n seconds.", 0, msecs / 1000);
        break;
    }
    case NetworkConnectionClosedReason: {
        bool isPrimaryStream = QnNetworkIssueBusinessEvent::decodePrimaryStream(reasonParamsEncoded, true);
        if (isPrimaryStream)
            result = tr("Connection to camera primary stream was unexpectedly closed.");
        else
            result = tr("Connection to camera secondary stream was unexpectedly closed.");
        break;
    }
    case NetworkRtpPacketLossReason: {
        QnNetworkIssueBusinessEvent::PacketLossSequence seq = QnNetworkIssueBusinessEvent::decodePacketLossSequence(reasonParamsEncoded);
        if (seq.valid)
            result = tr("RTP packet loss detected, prev seq.=%1 next seq.=%2.").arg(seq.prev).arg(seq.next);
        else
            result = tr("RTP packet loss detected.");
        break;
    }
    case ServerTerminatedReason: {
        result = tr("Server terminated.");
        break;
    }
    case ServerStartedReason: {
        result = tr("Server started after crash.");
        break;
    }
    case StorageIoErrorReason: {
        QString storageUrl = reasonParamsEncoded;
        result = tr("I/O error has occurred at %1.").arg(storageUrl);
        break;
    }
    case StorageTooSlowReason: {
        QString storageUrl = reasonParamsEncoded;
        result = tr("Not enough HDD/SSD speed for recording to %1.").arg(storageUrl);
        break;
    }
    case StorageFullReason: {
        QString storageUrl = reasonParamsEncoded;
        result = tr("HDD/SSD disk %1 is full. Disk contains too much data that is not managed by VMS.").arg(storageUrl);
        break;
    }
    case LicenseRemoved: {
        QString disabledCameras = reasonParamsEncoded;
        result = tr("Recording on %n camera(s) is disabled: ", NULL, disabledCameras.split(L',').size());
        result += disabledCameras;
        break;
    }
    default:
        break;
    }

    return result;
}

QString QnBusinessStringsHelper::aggregatedEventDetails(const QnAbstractBusinessActionPtr& action,
                                              const QnBusinessAggregationInfo& aggregationInfo,
                                              const QString& delimiter) {
    QString result;
    if (aggregationInfo.isEmpty()) {
        result += eventDetailsWithTimestamp(action->getRuntimeParams(), action->getAggregationCount(), delimiter);
    }

    for (const QnInfoDetail& detail: aggregationInfo.toList()) {
        if (!result.isEmpty())
            result += delimiter;
        result += eventDetailsWithTimestamp(detail.runtimeParams(), detail.count(), delimiter);
    }
    return result;
}

QVariantList QnBusinessStringsHelper::aggregatedEventDetailsMap(const QnAbstractBusinessActionPtr& action,
                                              const QnBusinessAggregationInfo& aggregationInfo,
                                              bool useIp) {
    QVariantList result;
    if (aggregationInfo.isEmpty()) {
        result << eventDetailsMap(action, QnInfoDetail(action->getRuntimeParams(), action->getAggregationCount()), useIp);
    }

    for (const QnInfoDetail& detail: aggregationInfo.toList()) {
        result << eventDetailsMap(action, detail, useIp);
    }
    return result;
}

QVariantList QnBusinessStringsHelper::aggregatedEventDetailsMap(
    const QnAbstractBusinessActionPtr& action,
    const QList<QnInfoDetail>& aggregationDetailList,
    bool useIp )
{
    QVariantList result;
    for (const QnInfoDetail& detail: aggregationDetailList)
        result << eventDetailsMap(action, detail, useIp);
    return result;
}

QString QnBusinessStringsHelper::motionUrl(const QnBusinessEventParameters &params, bool /*isPublic*/) {
    QnUuid id = params.eventResourceId;
    QnNetworkResourcePtr res = !id.isNull() ? 
                            qnResPool->getResourceById(id).dynamicCast<QnNetworkResource>() : 
                            QnNetworkResourcePtr();
    if (!res)
        return QString();

    QnResourcePtr mserverRes = res->getParentResource();
    if (!mserverRes)
        return QString();

    QUrl appServerUrl = QnAppServerConnectionFactory::url();
    quint64 ts = params.eventTimestamp;

    QnCameraHistoryPtr history = QnCameraHistoryPool::instance()->getCameraHistory(res);
    if (history) {
        QnMediaServerResourcePtr newServer = history->getMediaServerOnTime(ts/1000, false);
        if (newServer)
            mserverRes = newServer;
    }

    if (resolveAddress(appServerUrl.host()) == QHostAddress::LocalHost) {
        QUrl mserverUrl = mserverRes->getUrl();
        appServerUrl.setHost(mserverUrl.host());
    }

    QString result(lit("https://%1:%2/web/camera?physical_id=%3&pos=%4"));
    result = result.arg(appServerUrl.host()).arg(appServerUrl.port(80)).arg(res->getPhysicalId()).arg(ts/1000);

    return result;
}

