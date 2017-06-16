#include "business_strings_helper.h"
#include <utils/common/app_info.h>

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <business/business_aggregation_info.h>
#include <business/business_event_rule.h>
#include <business/events/reasoned_business_event.h>
#include <business/events/network_issue_business_event.h>
#include <business/events/camera_input_business_event.h>
#include <business/events/conflict_business_event.h>
#include <business/events/ip_conflict_business_event.h>
#include <business/events/mserver_conflict_business_event.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_history.h>
#include <utils/common/id.h>
#include <nx/network/nettools.h> /* For resolveAddress. */

QnBusinessStringsHelper::QnBusinessStringsHelper(QnCommonModule* commonModule):
    QObject(),
    QnCommonModuleAware(commonModule)
{
}

QString QnBusinessStringsHelper::actionName(QnBusiness::ActionType value) const
{
    using namespace QnBusiness;

    switch (value)
    {
        case UndefinedAction:           return QString();
        case CameraOutputAction:        return QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Device output"),
            tr("Camera output")
        );
        case CameraRecordingAction:     return QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Device recording"),
            tr("Camera recording")
        );
        case BookmarkAction:            return tr("Bookmark");
        case PanicRecordingAction:      return tr("Panic recording");
        case SendMailAction:            return tr("Send Email");
        case DiagnosticsAction:         return tr("Write to log");
        case ShowPopupAction:           return tr("Show notification");
        case PlaySoundAction:           return tr("Repeat sound");
        case PlaySoundOnceAction:       return tr("Play sound");
        case SayTextAction:             return tr("Speak");
        case ExecutePtzPresetAction:    return tr("Execute PTZ preset");
        case ShowTextOverlayAction:     return tr("Show text overlay");
        case ShowOnAlarmLayoutAction:   return tr("Show on Alarm Layout");
        case ExecHttpRequestAction:     return tr("Do HTTP request");

        default:
            break;
    }

    NX_ASSERT(false, Q_FUNC_INFO, "All enumeration values must be handled here");
    return lit("Unknown (%1)").arg(static_cast<int>(value));
}

QString QnBusinessStringsHelper::eventName(QnBusiness::EventType value, int count) const
{
    using namespace QnBusiness;

    if (value >= UserDefinedEvent)
    {
        QString result = tr("Generic Event");
        if (value > UserDefinedEvent)
            result += lit(" (%1)").arg((int)value - (int)UserDefinedEvent); // reserved for future use
        return result;
    }

    switch (value)
    {
        case CameraMotionEvent:     return tr("Motion on Cameras", "", count);
        case CameraInputEvent:      return QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Input Signal on Devices", "", count),
            tr("Input Signal on Cameras", "", count)
        );
        case CameraDisconnectEvent: return QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Devices Disconnected", "", count),
            tr("Cameras Disconnected", "", count)
        );
        case CameraIpConflictEvent: return QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Devices IP Conflict", "", count),
            tr("Cameras IP Conflict", "", count)
        );
        case AnyCameraEvent:        return QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Any Device Issue"),
            tr("Any Camera Issue")
        );
        case StorageFailureEvent:   return tr("Storage Failure");
        case NetworkIssueEvent:     return tr("Network Issue");
        case ServerFailureEvent:    return tr("Server Failure");
        case ServerConflictEvent:   return tr("Server Conflict");
        case ServerStartEvent:      return tr("Server Started");
        case LicenseIssueEvent:     return tr("License Issue");
        case BackupFinishedEvent:   return tr("Archive backup finished");

        case AnyServerEvent:        return tr("Any Server Issue");
        case AnyBusinessEvent:      return tr("Any Event");

        case SoftwareTriggerEvent:  return tr("Software Trigger");

        default:
            return QString();
    }
}

QString QnBusinessStringsHelper::eventAtResource(const QnBusinessEventParameters &params,
    Qn::ResourceInfoLevel detailLevel) const
{
    using namespace QnBusiness;

    QnResourcePtr source = eventSource(params);
    QnVirtualCameraResourcePtr camera = source.dynamicCast<QnVirtualCameraResource>();
    QString resourceName = QnResourceDisplayInfo(source).toString(detailLevel);
    switch (params.eventType) {
    case UndefinedEvent:
        return tr("Undefined event has occurred on %1").arg(resourceName);

    case CameraDisconnectEvent:
        return QnDeviceDependentStrings::getNameFromSet(
            resourcePool(),
            QnCameraDeviceStringSet(
                tr("Device %1 was disconnected"),
                tr("Camera %1 was disconnected"),
                tr("I/O Module %1 was disconnected")
            ), camera
        ).arg(resourceName);

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
        return QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
    		//: Device IP Conflict at <server_name>
            tr("Device IP Conflict at %1"),

            //: Camera IP Conflict at <server_name>
            tr("Camera IP Conflict at %1")
        ).arg(resourceName);

    case ServerConflictEvent:
        return tr("Server \"%1\" Conflict").arg(resourceName);

    case ServerStartEvent:
        return tr("Server \"%1\" Started").arg(resourceName);
    case LicenseIssueEvent:
        return tr("Server \"%1\" has a license problem").arg(resourceName);
    case BackupFinishedEvent:
        return tr("Server \"%1\" has finished an archive backup").arg(resourceName);
    case UserDefinedEvent:
        return (!params.caption.isEmpty() ? params.caption
            : (params.resourceName.isEmpty() ? tr("Generic Event")
                : tr("Generic Event at %1").arg(params.resourceName)));
    case SoftwareTriggerEvent:
        return tr("Software trigger %1 at %2")
            .arg(getSoftwareTriggerName(params))
            .arg(resourceName);
    default:
        break;
    }
    return tr("An unknown event has occurred");
}

QString QnBusinessStringsHelper::eventAtResources(const QnBusinessEventParameters& params) const
{
    if (params.eventType == QnBusiness::SoftwareTriggerEvent)
    {
        return tr("Software Trigger %1 has been activated multiple times")
            .arg(getSoftwareTriggerName(params));
    }

    return tr("Multiple %1 events have occured").arg(eventName(params.eventType));
}

QString QnBusinessStringsHelper::getResoureNameFromParams(const QnBusinessEventParameters& params,
    Qn::ResourceInfoLevel detailLevel) const
{
    QString result = QnResourceDisplayInfo(eventSource(params)).toString(detailLevel);
    return result.isEmpty() ? params.resourceName : result;
}

QString QnBusinessStringsHelper::getResoureIPFromParams(
    const QnBusinessEventParameters& params) const
{
	QString result = QnResourceDisplayInfo(eventSource(params)).host();
	return result.isNull() ? params.resourceName : result;
}

QStringList QnBusinessStringsHelper::eventDescription(const QnAbstractBusinessActionPtr& action,
    const QnBusinessAggregationInfo &aggregationInfo,
    Qn::ResourceInfoLevel detailLevel) const
{
    QStringList result;

    QnBusinessEventParameters params = action->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    result << tr("Event: %1").arg(eventName(eventType));

    QString sourceText = getResoureNameFromParams(params, detailLevel);
    if (!sourceText.isEmpty())
        result << tr("Source: %1").arg(sourceText);

    if (eventType >= QnBusiness::UserDefinedEvent)
    {
        if (!params.caption.isEmpty())
            result << tr("Caption: %1").arg(params.caption);
    }

    const auto details = aggregatedEventDetails(action, aggregationInfo);
    result << details;

    return result;
}

QStringList QnBusinessStringsHelper::eventDetailsWithTimestamp(
    const QnBusinessEventParameters &params, int aggregationCount) const
{
    return QStringList()
        << eventTimestamp(params, aggregationCount)
        << eventDetails(params);
}

QStringList QnBusinessStringsHelper::eventDetails(const QnBusinessEventParameters& params) const
{
    using namespace QnBusiness;

    QStringList result;

    switch (params.eventType) {
    case CameraInputEvent: {
        result << tr("Input Port: %1").arg(params.inputPortId);
        break;
    }
    case StorageFailureEvent:
    case NetworkIssueEvent:
    case ServerFailureEvent:
    case LicenseIssueEvent:
    case BackupFinishedEvent:
    {
        result << tr("Reason: %1").arg(eventReason(params));
        break;
    }
    case CameraIpConflictEvent:
    {
        result << tr("Conflicting Address: %1").arg(params.caption);
        int n = 0;
        for (const QString& mac: params.description.split(QnIPConflictBusinessEvent::Delimiter))
            result << tr("MAC #%1: %2").arg(++n).arg(mac);

        break;
    }
    case ServerConflictEvent:
    {
        if (!params.description.isEmpty())
        {
            QnCameraConflictList conflicts;
            conflicts.sourceServer = params.caption;
            conflicts.decode(params.description);
            int n = 0;
            for (auto itr = conflicts.camerasByServer.begin(); itr != conflicts.camerasByServer.end(); ++itr)
            {
                const QString &server = itr.key();
                //: Conflicting Server #5: 10.0.2.1
                result << tr("Conflicting Server #%1: %2").arg(++n).arg(server);
                int m = 0;
                //: MAC #2: D0-50-99-38-1E-12
                for (const QString &camera: conflicts.camerasByServer[server])
                    result << tr("MAC #%1: %2").arg(++m).arg(camera);


            }
        }
        else
        {
            result << tr("Conflicting Server: %1").arg(params.caption);
        }
        break;
    }
    case ServerStartEvent:
        break;
    case UserDefinedEvent:
        if (!params.description.isEmpty())
            result << params.description;
        break;
    case SoftwareTriggerEvent:
        result << tr("Trigger: %1").arg(getSoftwareTriggerName(params));
        break;
    default:
        break;
    }

    return result;
}

QString QnBusinessStringsHelper::eventTimestampShort(const QnBusinessEventParameters &params,
    int aggregationCount) const
{
    quint64 ts = params.eventTimestampUsec;
    QDateTime time = QDateTime::fromMSecsSinceEpoch(ts/1000);

    int count = qMax(aggregationCount, 1);
    if (count == 1)
        return tr("%2 <b>%1</b>", "%1 means time, %2 means date")
            .arg(time.time().toString(Qt::DefaultLocaleShortDate))
            .arg(time.date().toString(Qt::DefaultLocaleShortDate));
    else
        return tr("%n times, first: %2 <b>%1</b>", "%1 means time, %2 means date", count)
            .arg(time.time().toString(Qt::DefaultLocaleShortDate))
            .arg(time.date().toString(Qt::DefaultLocaleShortDate));
}

QString QnBusinessStringsHelper::eventTimestamp(const QnBusinessEventParameters &params,
    int aggregationCount) const
{
    quint64 ts = params.eventTimestampUsec;
    QDateTime time = QDateTime::fromMSecsSinceEpoch(ts/1000);

    int count = qMax(aggregationCount, 1);
    if (count == 1)
        return tr("Time: %1 on %2", "%1 means time, %2 means date")
            .arg(time.time().toString(Qt::DefaultLocaleShortDate))
            .arg(time.date().toString(Qt::DefaultLocaleShortDate));
    else
        return tr("First occurrence: %1 on %2 (%n times total)", "%1 means time, %2 means date", count)
            .arg(time.time().toString(Qt::DefaultLocaleShortDate))
            .arg(time.date().toString(Qt::DefaultLocaleShortDate));
}

QString QnBusinessStringsHelper::eventTimestampDate(const QnBusinessEventParameters &params) const
{
	quint64 ts = params.eventTimestampUsec;
	QDateTime time = QDateTime::fromMSecsSinceEpoch(ts / 1000);
	return time.date().toString(Qt::DefaultLocaleShortDate);
}

QString QnBusinessStringsHelper::eventTimestampTime(const QnBusinessEventParameters &params) const
{
	quint64 ts = params.eventTimestampUsec;
	QDateTime time = QDateTime::fromMSecsSinceEpoch(ts / 1000);
	return time.time().toString(Qt::DefaultLocaleShortDate);
}

QnResourcePtr QnBusinessStringsHelper::eventSource(const QnBusinessEventParameters &params) const
{
    QnUuid id = params.eventResourceId;
    return !id.isNull()
        ? resourcePool()->getResourceById(id)
        : QnResourcePtr();
}

QString QnBusinessStringsHelper::eventReason(const QnBusinessEventParameters& params) const
{
    using namespace QnBusiness;

    QString reasonParamsEncoded = params.description;

    QString result;
    switch (params.reasonCode)
    {
    case NetworkNoFrameReason:
    {
        int msecs = QnNetworkIssueBusinessEvent::decodeTimeoutMsecs(reasonParamsEncoded, 5000);
        result = tr("No data received during last %n seconds.", "", msecs / 1000);
        break;
    }
    case NetworkConnectionClosedReason:
    {
        bool isPrimaryStream = QnNetworkIssueBusinessEvent::decodePrimaryStream(reasonParamsEncoded, true);

        QnVirtualCameraResourcePtr camera = eventSource(params).dynamicCast<QnVirtualCameraResource>();
        if (camera && !camera->hasVideo(nullptr))
            result = tr("Connection to device was unexpectedly closed.");
        else if (isPrimaryStream)
            result = tr("Connection to camera (primary stream) was unexpectedly closed.");
        else
            result = tr("Connection to camera (secondary stream) was unexpectedly closed.");
        break;
    }
    case NetworkRtpPacketLossReason:
    {
        QnNetworkIssueBusinessEvent::PacketLossSequence seq = QnNetworkIssueBusinessEvent::decodePacketLossSequence(reasonParamsEncoded);
        if (seq.valid)
            result = tr("RTP packet loss detected, prev seq.=%1 next seq.=%2.").arg(seq.prev).arg(seq.next);
        else
            result = tr("RTP packet loss detected.");
        break;
    }
    case NetworkNoResponseFromDevice:
    {
        return tr("Device does not respond to network requests.");
    }
    case ServerTerminatedReason:
    {
        result = tr("Connection to server is lost.");
        break;
    }
    case ServerStartedReason:
    {
        result = tr("Server stopped unexpectedly.");
        break;
    }
    case StorageIoErrorReason:
    {
        QString storageUrl = reasonParamsEncoded;
        result = tr("I/O error has occurred at %1.").arg(storageUrl);
        break;
    }
    case StorageTooSlowReason:
    {
        QString storageUrl = reasonParamsEncoded;
        result = tr("Not enough HDD/SSD speed for recording to %1.").arg(storageUrl);
        break;
    }
    case StorageFullReason:
    {
        QString storageUrl = reasonParamsEncoded;
        result = tr("HDD/SSD disk \"%1\" is full. Disk contains too much data that is not managed by VMS.").arg(storageUrl);
        break;
    }
    case SystemStorageFullReason:
    {
        QString storageUrl = reasonParamsEncoded;
        result = tr("System disk \"%1\" is almost full.").arg(storageUrl);
        break;
    }
    case BackupFailedNoBackupStorageError:
    {
        result = tr("Archive backup failed: No available backup storages with sufficient free space");
        break;
    }
    case BackupFailedSourceStorageError:
    {
        result = tr("Archive backup failed: Target storage failure");
        break;
    }
    case BackupFailedSourceFileError:
    {
        result = tr("Archive backup failed: Source file open/read error");
        break;
    }
    case BackupFailedTargetFileError:
    {
        result = tr("Archive backup failed: Target file create/write error");
        break;
    }
    case BackupFailedChunkError:
    {
        result = tr("Archive backup failed: File catalog error");
        break;
    }
    case BackupEndOfPeriod:
    {
        qint64 timeStampMs = params.description.toLongLong();
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(timeStampMs);
        // todo: #gdm add server/client timezone conversion
        result = tr("Archive backup finished, but is not fully completed because backup time is over. Data is backed up to %1").arg(dt.toString(Qt::DefaultLocaleShortDate));
    }
    case BackupDone:
    {
        result = tr("Archive backup is successfully completed");
        break;
    }
    case BackupCancelled:
    {
        qint64 timeStampMs = params.description.toLongLong();
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(timeStampMs);
        // todo: #gdm add server/client timezone conversion
        result = tr("Archive backup is canceled by user. Data is backed up to %1").arg(dt.toString(Qt::DefaultLocaleShortDate));
        break;
    }

    case LicenseRemoved:
    {
        QnVirtualCameraResourceList disabledCameras;
        for (const QString &id: reasonParamsEncoded.split(L';'))
            if (const QnVirtualCameraResourcePtr &camera = resourcePool()->getResourceById<QnVirtualCameraResource>(QnUuid(id)))
                disabledCameras << camera;
        NX_ASSERT(!disabledCameras.isEmpty(), Q_FUNC_INFO, "At least one camera should be disabled on this event");

        result = QnDeviceDependentStrings::getNameFromSet(
                resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Not enough licenses. Recording has been disabled on following devices:"),
                    tr("Not enough licenses. Recording has been disabled on following cameras:"),
                    tr("Not enough licenses. Recording has been disabled on following I/O modules:")
                ),
                disabledCameras
            );

        break;
    }
    default:
        break;
    }

    return result;
}

QStringList QnBusinessStringsHelper::aggregatedEventDetails(
    const QnAbstractBusinessActionPtr& action,
    const QnBusinessAggregationInfo& aggregationInfo) const
{
    QStringList result;
    if (aggregationInfo.isEmpty())
    {
        result << eventDetailsWithTimestamp(action->getRuntimeParams(),
            action->getAggregationCount());
    }

    for (const QnInfoDetail& detail: aggregationInfo.toList())
        result << eventDetailsWithTimestamp(detail.runtimeParams(), detail.count());

    return result;
}

QString QnBusinessStringsHelper::urlForCamera(const QnUuid& id, qint64 timestampUsec,
    bool isPublic) const
{
    QnNetworkResourcePtr res = !id.isNull() ?
                            resourcePool()->getResourceById<QnNetworkResource>(id) :
                            QnNetworkResourcePtr();
    if (id.isNull())
        return QString();

    QnVirtualCameraResourcePtr camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
    if (!camera)
        return QString();

    QnMediaServerResourcePtr mserverRes = camera->getParentServer();
    if (!mserverRes)
        return QString();

    quint64 timeStampMs = timestampUsec / 1000;
    QnMediaServerResourcePtr newServer = cameraHistoryPool()->getMediaServerOnTime(camera, timeStampMs);
    if (newServer)
        mserverRes = newServer;

    if (const auto& connection = camera->commonModule()->ec2Connection())
    {
        QUrl appServerUrl = connection->connectionInfo().ecUrl;
        if (appServerUrl.host().isEmpty() || resolveAddress(appServerUrl.host()) == QHostAddress::LocalHost)
        {
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
        QString result(lit("http://%1:%2/static/index.html#/view/%3?time=%4"));
        result = result.arg(appServerUrl.host()).arg(appServerUrl.port(80)).arg(camera->getUniqueId()).arg(timeStampMs);
        return result;
    }

    return QString();
}

QString QnBusinessStringsHelper::toggleStateToString(QnBusiness::EventState state) const
{
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

QString QnBusinessStringsHelper::eventTypeString(
        QnBusiness::EventType eventType,
        QnBusiness::EventState eventState,
        QnBusiness::ActionType actionType,
        const QnBusinessActionParameters &actionParams) const
{
    QString typeStr = QnBusinessStringsHelper::eventName(eventType);
    if (QnBusiness::isActionProlonged(actionType, actionParams))
        return tr("While %1").arg(typeStr);
    else
        return tr("On %1 %2").arg(typeStr).arg(toggleStateToString(eventState));
}

QString QnBusinessStringsHelper::bruleDescriptionText(const QnBusinessEventRulePtr& bRule) const
{
    QString eventString = eventTypeString(bRule->eventType(), bRule->eventState(), bRule->actionType(), bRule->actionParams());
    return lit("%1 --> %2").arg(eventString).arg(actionName(bRule->actionType()));
}

QString QnBusinessStringsHelper::defaultSoftwareTriggerName()
{
    return tr("Default Trigger");
}

QString QnBusinessStringsHelper::getSoftwareTriggerName(const QString& id)
{
    const auto triggerId = id.trimmed();
    return triggerId.isEmpty() ? defaultSoftwareTriggerName() : triggerId;
}

QString QnBusinessStringsHelper::getSoftwareTriggerName(const QnBusinessEventParameters& params)
{
    NX_ASSERT(params.eventType == QnBusiness::SoftwareTriggerEvent);
    return getSoftwareTriggerName(params.caption);
}

QString QnBusinessStringsHelper::actionSubjects(const QnBusinessEventRulePtr& rule,
    bool noSingleName) const
{
    QnUserResourceList users;
    QList<QnUuid> roles;

    if (QnBusiness::requiresUserResource(rule->actionType()))
        userRolesManager()->usersAndRoles(rule->actionResources(), users, roles);
    else
        userRolesManager()->usersAndRoles(rule->actionParams().additionalResources, users, roles);

    return actionSubjects(users, roles, noSingleName);
}

QString QnBusinessStringsHelper::actionSubjects(
    const QnUserResourceList& users,
    const QList<QnUuid>& roles,
    bool noSingleName) const
{
    if (users.empty() && roles.empty())
        return tr("All Users");

    if (!noSingleName)
    {
        if (users.size() == 1 && roles.empty())
            return users.front()->getName();

        if (users.empty() && roles.size() == 1)
        {
            return lit("%1 %2 %3")
                .arg(tr("Role"))
                .arg(QChar(L'\x2013')) //< En-dash.
                .arg(userRolesManager()->userRoleName(roles.front()));
        }
    }

    if (roles.empty())
        return tr("%n Users", "", users.size());

    if (!users.empty())
    {
        return lit("%1, %2")
            .arg(tr("%n Roles", "", roles.size()))
            .arg(tr("%n Users", "", users.size()));
    }

    static const QSet<QnUuid> kAdminRoles = QnUserRolesManager::adminRoleIds().toSet();
    if (roles.toSet() == kAdminRoles)
        return tr("All Administrators");

    return tr("%n Roles", "", roles.size());
}
