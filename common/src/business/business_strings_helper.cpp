#include "business_strings_helper.h"
#include "version.h"

#include <api/app_server_connection.h>

#include <business/business_aggregation_info.h>
#include <business/events/reasoned_business_event.h>
#include <business/events/network_issue_business_event.h>
#include <business/events/camera_input_business_event.h>
#include <business/events/conflict_business_event.h>

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>
#include <core/resource/network_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include "utils/common/id.h"

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
    case SendMailAction:            return tr("Send mail");
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
    case ServerFailureEvent:    return tr("Media Server Failure");
    case ServerConflictEvent:   return tr("Media Server Conflict");
    case ServerStartEvent:      return tr("Media Server Started");
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
    switch (params.getEventType()) {
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
        return tr("Media Server \"%1\" Failure").arg(resourceName);

    case CameraIpConflictEvent:
        return tr("Camera IP Conflict at %1").arg(resourceName);

    case ServerConflictEvent:
        return tr("Media Server \"%1\" Conflict").arg(resourceName);

    case ServerStartEvent:
        return tr("Media Server \"%1\" Started").arg(resourceName);

    default:
        break;
    }
    return tr("Unknown event has occurred");
}

QString QnBusinessStringsHelper::eventDescription(const QnAbstractBusinessActionPtr& action, const QnBusinessAggregationInfo &aggregationInfo, bool useIp, bool useHtml) {

    QString delimiter = useHtml
            ? htmlDelimiter
            : plainTextDelimiter;

    QnBusinessEventParameters params = action->getRuntimeParams();
    QnBusiness::EventType eventType = params.getEventType();

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
    QnBusiness::EventType eventType = params.getEventType();

    QVariantHash contextMap;

    contextMap[tpProductName] = lit(QN_PRODUCT_NAME_LONG);
    contextMap[tpEvent] = eventName(eventType);
    contextMap[tpSource] = eventSource(params, useIp);
    if (eventType == QnBusiness::CameraMotionEvent) {
        contextMap[tpUrlInt] = motionUrl(params, false);
        contextMap[tpUrlExt] = motionUrl(params, true);
    }
    contextMap[tpAggregated] = aggregatedEventDetailsMap(action, aggregationInfo);

    return contextMap;
}

QString QnBusinessStringsHelper::eventDetailsWithTimestamp(const QnBusinessEventParameters &params, int aggregationCount, const QString& delimiter) {
    return eventTimestamp(params, aggregationCount) + delimiter + eventDetails(params, aggregationCount, delimiter);
}

QString QnBusinessStringsHelper::eventDetails(const QnBusinessEventParameters &params, int aggregationCount, const QString& delimiter) {
    using namespace QnBusiness;

    Q_UNUSED(aggregationCount)
    QString result;

    switch (params.getEventType()) {
    case CameraInputEvent: {
        result = tr("Input port: %1").arg(params.getInputPortId());
        break;
    }
    case StorageFailureEvent:
    case NetworkIssueEvent:
    case ServerFailureEvent: {
        result += tr("Reason: %1").arg(eventReason(params));
        break;
    }
    case CameraIpConflictEvent: {
        result += tr("Conflict address: %1").arg(params.getSource());

        int n = 0;
        foreach (QString mac, params.getConflicts()) {
            result += delimiter;
            result += tr("Camera #%1 MAC: %2").arg(++n).arg(mac);
        }
        break;
    }
    case ServerConflictEvent: {
        QVariantList conflicts;
        int n = 0;
        foreach (QString ip, params.getConflicts()) {
            result += delimiter;
            result += tr("Conflicting EC #%1: %2").arg(n).arg(ip);
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

QVariantHash QnBusinessStringsHelper::eventDetailsMap(const QnBusinessEventParameters &params, int aggregationCount) {
    using namespace QnBusiness;

    QVariantHash detailsMap;

    detailsMap[tpTimestamp] = eventTimestampShort(params, aggregationCount);

    switch (params.getEventType()) {
    case CameraInputEvent: {
        detailsMap[tpInputPort] = params.getInputPortId();
        break;
    }
    case StorageFailureEvent:
    case NetworkIssueEvent:
    case ServerFailureEvent: {
        detailsMap[tpReason] = eventReason(params);
        break;
    }
    case CameraIpConflictEvent: {
        detailsMap[lit("cameraConflictAddress")] = params.getSource();
        QVariantList conflicts;
        int n = 0;
        foreach (QString mac, params.getConflicts()) {
            QVariantHash conflict;
            conflict[lit("number")] = ++n;
            conflict[lit("mac")] = mac;
            conflicts << conflict;
        }
        detailsMap[lit("cameraConflicts")] = conflicts;

        break;
    }
    case ServerConflictEvent: {
        QVariantList conflicts;
        int n = 0;
        foreach (QString ip, params.getConflicts()) {
            QVariantHash conflict;
            conflict[lit("number")] = ++n;
            conflict[lit("ip")] = ip;
            conflicts << conflict;
        }
        detailsMap[lit("msConflicts")] = conflicts;
        break;
    }
    default:
        break;
    }
    return detailsMap;
}


QString QnBusinessStringsHelper::eventTimestampShort(const QnBusinessEventParameters &params, int aggregationCount) {
    quint64 ts = params.getEventTimestamp();
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
    quint64 ts = params.getEventTimestamp();
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
    QnId id = params.getEventResourceId();
    QnResourcePtr res = !id.isNull() ? qnResPool->getResourceById(id) : QnResourcePtr();
    return getFullResourceName(res, useIp);
}

QString QnBusinessStringsHelper::eventReason(const QnBusinessEventParameters& params) {
    using namespace QnBusiness;
    
    QString reasonParamsEncoded = params.getReasonParamsEncoded();

    QString result;
    switch (params.getReasonCode()) {
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
    case StorageNotEnoughSpaceReason: {
        QString storageUrl = reasonParamsEncoded;
        result = tr("HDD/SSD disk %1 is full. Disk contains too much data that is not managed by VMS.").arg(storageUrl);
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

    foreach (QnInfoDetail detail, aggregationInfo.toList()) {
        if (!result.isEmpty())
            result += delimiter;
        result += eventDetailsWithTimestamp(detail.runtimeParams, detail.count, delimiter);
    }
    return result;
}

QVariantList QnBusinessStringsHelper::aggregatedEventDetailsMap(const QnAbstractBusinessActionPtr& action,
                                              const QnBusinessAggregationInfo& aggregationInfo) {
    QVariantList result;
    if (aggregationInfo.isEmpty()) {
        result << eventDetailsMap(action->getRuntimeParams(), action->getAggregationCount());
    }

    foreach (QnInfoDetail detail, aggregationInfo.toList()) {
        result << eventDetailsMap(detail.runtimeParams, detail.count);
    }
    return result;
}

QString QnBusinessStringsHelper::motionUrl(const QnBusinessEventParameters &params, bool isPublic) {
    QnId id = params.getEventResourceId();
    QnNetworkResourcePtr res = !id.isNull() ? 
                            qnResPool->getResourceById(id).dynamicCast<QnNetworkResource>() : 
                            QnNetworkResourcePtr();
    if (!res)
        return QString();

    QnResourcePtr mserverRes = res->getParentResource();
    if (!mserverRes)
        return QString();

    QUrl appServerUrl = QnAppServerConnectionFactory::publicUrl();
    QUrl appServerDefaultUrl = QnAppServerConnectionFactory::defaultUrl();
    quint64 ts = params.getEventTimestamp();

    QnCameraHistoryPtr history = QnCameraHistoryPool::instance()->getCameraHistory(res->getPhysicalId());
    if (history) {
        QnTimePeriod period;
        QnMediaServerResourcePtr newServer = history->getMediaServerOnTime(ts/1000, true, period, false);
        if (newServer)
            mserverRes = newServer;
    }

    if (!isPublic || resolveAddress(appServerUrl.host()) == QHostAddress::LocalHost) {
        if (resolveAddress(appServerDefaultUrl.host()) != QHostAddress::LocalHost) {
            appServerUrl = appServerDefaultUrl;
        } else {
            QUrl mserverUrl = mserverRes->getUrl();
            appServerUrl.setHost(mserverUrl.host());
        }
    }

    QString result(lit("https://%1:%2/web/camera?physical_id=%3&pos=%4"));
    result = result.arg(appServerUrl.host()).arg(appServerUrl.port(80)).arg(res->getPhysicalId()).arg(ts/1000);

    return result;
}

