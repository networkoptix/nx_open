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
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include "core/resource/camera_history.h"
#include "events/ip_conflict_business_event.h"
#include "business_event_rule.h"


namespace {
    static const QString plainTextDelimiter(lit("\n"));
    static const QString htmlDelimiter(lit("<br>"));
}

QString QnBusinessStringsHelper::actionName(QnBusiness::ActionType value) {
    using namespace QnBusiness;

    /* Do not use 'default' keyword! 
     * Warning should be raised on unknown enumeration values. */
    switch(value) {
    case UndefinedAction:           return QString();
    case CameraOutputAction:        return tr("%1 output").arg(getDefaultDeviceNameUpper());
    case CameraOutputOnceAction:    return tr("%1 output for 30 sec").arg(getDefaultDeviceNameUpper());
    case BookmarkAction:            return tr("Bookmark");
    case CameraRecordingAction:     return tr("%1 recording").arg(getDefaultDeviceNameUpper());
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

    if (value >= UserDefinedEvent) {
        QString result = tr("Generic Event");
        if (value > UserDefinedEvent)
            result += tr(" (%1)").arg((int)value - (int)UserDefinedEvent); // reserved for future use
        return result;
    }

    switch( value )
    {
    case CameraMotionEvent:     return tr("Motion on Camera");
    case CameraInputEvent:      return tr("Input Signal on %1").arg(getDefaultDeviceNameUpper());
    case CameraDisconnectEvent: return tr("%1 Disconnected").arg(getDefaultDeviceNameUpper());
    case StorageFailureEvent:   return tr("Storage Failure");
    case NetworkIssueEvent:     return tr("Network Issue");
    case CameraIpConflictEvent: return tr("%1 IP Conflict").arg(getDefaultDeviceNameUpper());
    case ServerFailureEvent:    return tr("Server Failure");
    case ServerConflictEvent:   return tr("Server Conflict");
    case ServerStartEvent:      return tr("Server Started");
    case LicenseIssueEvent:     return tr("License Issue");
    case AnyCameraEvent:        return tr("Any %1 Issue").arg(getDefaultDeviceNameUpper());
    case AnyServerEvent:        return tr("Any Server Issue");
    case AnyBusinessEvent:      return tr("Any Event");
    default:
        return QString();
    }
}

QString QnBusinessStringsHelper::eventAtResource(const QnBusinessEventParameters &params, bool useIp) {
    using namespace QnBusiness;

    QnResourcePtr source = eventSource(params);
    QnVirtualCameraResourcePtr camera = source.dynamicCast<QnVirtualCameraResource>();
    QString resourceName = getFullResourceName(source, useIp);
    switch (params.eventType) {
    case UndefinedEvent:
        return tr("Undefined event has occurred on %1").arg(resourceName);

    case CameraDisconnectEvent:
        //: Camera <camera_name> was disconnected
        return tr("%1 %2 was disconnected")
            .arg(getDefaultDeviceNameUpper(camera))
            .arg(resourceName);

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
        return tr("%1 IP Conflict at %2")
            .arg(getDefaultDeviceNameUpper(camera))
            .arg(resourceName);

    case ServerConflictEvent:
        return tr("Server \"%1\" Conflict").arg(resourceName);

    case ServerStartEvent:
        return tr("Server \"%1\" Started").arg(resourceName);
    case LicenseIssueEvent:
        return tr("Server \'%1\' has a license problem").arg(resourceName);
    case UserDefinedEvent:
        return !params.caption.isEmpty() ? params.caption :
               !params.description.isEmpty() ? params.description : resourceName;
    default:
        break;
    }
    return tr("An unknown event has occurred");
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

    QString sourceText = getFullResourceName(eventSource(params), useIp);
    if (sourceText.isNull())
        sourceText = params.resourceName;
    if (!sourceText.isEmpty()) {
        result += delimiter;
        result += tr("Source: %1").arg(sourceText);
    }

    if (eventType == QnBusiness::UserDefinedEvent) {
        if (!params.caption.isEmpty()) {
            result += delimiter;
            result += tr("Caption: %1").arg(params.caption);
        }
    }

    if (useHtml && eventType == QnBusiness::CameraMotionEvent) {
        result += delimiter;
        result += tr("Url: %1").arg(urlForCamera(params.eventResourceId, params.eventTimestampUsec, true));
    }

    result += delimiter;
    result += aggregatedEventDetails(action, aggregationInfo, delimiter);

    return result;
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
        result = tr("Input Port: %1").arg(params.inputPortId);
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
        result += tr("Conflict Address: %1").arg(params.caption);
        result += delimiter;
        int n = 0;
        for (const QString& mac: params.description.split(QnIPConflictBusinessEvent::Delimiter)) {
            result += delimiter;
            result += tr("MAC #%1: %2 ").arg(++n).arg(mac);
        }
        break;
    }
    case ServerConflictEvent: {
        if (!params.description.isEmpty()) {
            QnCameraConflictList conflicts;
            conflicts.sourceServer = params.caption;
            conflicts.decode(params.description);
            int n = 0;
            for (auto itr = conflicts.camerasByServer.begin(); itr != conflicts.camerasByServer.end(); ++itr) {
                const QString &server = itr.key();
                result += delimiter;
                result += tr("Conflicting Server #%1: %2").arg(++n).arg(server);
                int m = 0;
                for (const QString &camera: conflicts.camerasByServer[server]) {
                    result += delimiter;
                    result += tr("MAC #%1: %2 ").arg(++m).arg(camera);
                }

            }
        } else {
            result += tr("Conflicting Server: %1").arg(params.caption);
        }
        break;
    }
    case ServerStartEvent: 
        break;
    case UserDefinedEvent:
        result += !params.description.isEmpty() ? params.description : params.caption;
        break;
    default:
        break;
    }
    return result;
}

QString QnBusinessStringsHelper::eventTimestampShort(const QnBusinessEventParameters &params, int aggregationCount) {
    quint64 ts = params.eventTimestampUsec;
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
    quint64 ts = params.eventTimestampUsec;
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

QnResourcePtr QnBusinessStringsHelper::eventSource(const QnBusinessEventParameters &params) {
    QnUuid id = params.eventResourceId;
    return !id.isNull() 
        ? qnResPool->getResourceById(id) 
        : QnResourcePtr();
}

QString QnBusinessStringsHelper::eventReason(const QnBusinessEventParameters& params) {
    using namespace QnBusiness;
    
    QString reasonParamsEncoded = params.description;

    QString result;
    switch (params.reasonCode) {
    case NetworkNoFrameReason: {
        int msecs = QnNetworkIssueBusinessEvent::decodeTimeoutMsecs(reasonParamsEncoded, 5000);
        result = tr("No data received during last %n seconds.", 0, msecs / 1000);
        break;
    }
    case NetworkConnectionClosedReason: {
        QnVirtualCameraResourcePtr camera = eventSource(params).dynamicCast<QnVirtualCameraResource>();
        bool isPrimaryStream = QnNetworkIssueBusinessEvent::decodePrimaryStream(reasonParamsEncoded, true);

        if (!camera->hasVideo(nullptr))
            result = tr("Connection to %1 was unexpectedly closed.").arg(getDefaultDeviceNameLower(camera));
        else if (isPrimaryStream)
            result = tr("Connection to camera (primary stream) was unexpectedly closed.");
        else
            result = tr("Connection to camera (secondary stream) was unexpectedly closed.");
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
        result = tr("Connection to server is lost.");
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
        QnVirtualCameraResourceList disabledCameras;
        for (const QString &id: reasonParamsEncoded.split(L';'))
            if (const QnVirtualCameraResourcePtr &camera = qnResPool->getResourceById<QnVirtualCameraResource>(QnUuid(id))) 
                disabledCameras << camera;
        Q_ASSERT_X(!disabledCameras.isEmpty(), Q_FUNC_INFO, "At least one camera should be disabled on this event");
        
        result = tr("Recording on %1 is disabled: ").arg(getNumericDevicesName(disabledCameras, false));
        for (const auto &camera: disabledCameras)
            result += L'\n' + getFullResourceName(camera, true);
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

QString QnBusinessStringsHelper::urlForCamera(const QnUuid& id, qint64 timestampUsec, bool isPublic) {
    QnNetworkResourcePtr res = !id.isNull() ? 
                            qnResPool->getResourceById<QnNetworkResource>(id) : 
                            QnNetworkResourcePtr();
    if (id.isNull())
        return QString();

    QnVirtualCameraResourcePtr camera = qnResPool->getResourceById(id).dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return QString();

    QnMediaServerResourcePtr mserverRes = camera->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!mserverRes)
        return QString();
    
	quint64 timeStampMs = timestampUsec / 1000;
    QnMediaServerResourcePtr newServer = qnCameraHistoryPool->getMediaServerOnTime(camera, timeStampMs);
    if (newServer)
        mserverRes = newServer;

    QUrl appServerUrl = QnAppServerConnectionFactory::url();
    if (appServerUrl.host().isEmpty() || resolveAddress(appServerUrl.host()) == QHostAddress::LocalHost) {
        appServerUrl = mserverRes->getApiUrl();
        if (isPublic) {
            QString publicIP = mserverRes->getProperty(Qn::PUBLIC_IP);
            if (!publicIP.isEmpty()) {
                QStringList parts = publicIP.split(L':');
                appServerUrl.setHost(parts[0]);
                if (parts.size() > 1)
                    appServerUrl.setPort(parts[1].toInt());
            }
        }
    }

    QString result(lit("http://%1:%2/static/index.html/#/view/%3?time=%4"));
    result = result.arg(appServerUrl.host()).arg(appServerUrl.port(80)).arg(camera->getUniqueId()).arg(timeStampMs);

    return result;
}

QString QnBusinessStringsHelper::toggleStateToString(QnBusiness::EventState state) {
    switch (state) {
    case QnBusiness::ActiveState:
        return tr("start");
    case QnBusiness::InactiveState:
        return tr("stop");
    case QnBusiness::UndefinedState:
        return QString();
    default:
        break;
    }
    return QString();
}

QString QnBusinessStringsHelper::eventTypeString(QnBusiness::EventType eventType, QnBusiness::EventState eventState, QnBusiness::ActionType actionType) 
{
    QString typeStr = QnBusinessStringsHelper::eventName(eventType);
    if (QnBusiness::hasToggleState(actionType))
        return tr("While %1").arg(typeStr);
    else
        return tr("On %1 %2").arg(typeStr).arg(toggleStateToString(eventState));
}


QString QnBusinessStringsHelper::bruleDescriptionText(const QnBusinessEventRulePtr& bRule)
{
    QString eventString = eventTypeString(bRule->eventType(), bRule->eventState(), bRule->actionType());
    return tr("%1 --> %2").arg(eventString).arg(actionName(bRule->actionType()));
}
