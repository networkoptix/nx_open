#include "strings_helper.h"

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/network_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/camera_history.h>
#include <nx/network/nettools.h> /* For resolveAddress. */
#include <utils/common/app_info.h>
#include <utils/common/id.h>

#include <nx/api/analytics/driver_manifest.h>

#include <nx/vms/event/aggregation_info.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/events/events.h>

#include <nx/utils/log/assert.h>

namespace {

nx::api::AnalyticsEventType analyticsEventType(const QnVirtualCameraResourcePtr& camera,
    const QnUuid& driverId, const QnUuid& eventTypeId)
{
    NX_EXPECT(camera);
    if (!camera)
        return {};

    if (driverId.isNull())
        return {};

    auto server = camera->getParentServer();
    NX_EXPECT(server);
    if (!server)
        return {};

    const auto drivers = server->analyticsDrivers();
    const auto driver = std::find_if(drivers.cbegin(), drivers.cend(),
        [driverId](const nx::api::AnalyticsDriverManifest& manifest)
        {
            return manifest.driverId == driverId;
        });

    NX_EXPECT(driver != drivers.cend());
    if (driver == drivers.cend())
        return {};

    const auto types = driver->outputEventTypes;
    const auto eventType = std::find_if(types.cbegin(), types.cend(),
        [eventTypeId](const nx::api::AnalyticsEventType eventType)
        {
            return eventType.typeId == eventTypeId;
        });

    return eventType == types.cend()
        ? nx::api::AnalyticsEventType()
        : *eventType;
}

} // namespace



namespace nx {
namespace vms {
namespace event {

StringsHelper::StringsHelper(QnCommonModule* commonModule):
    QObject(),
    QnCommonModuleAware(commonModule)
{
}

QString StringsHelper::actionName(ActionType value) const
{
    switch (value)
    {
        case undefinedAction:         return QString();
        case bookmarkAction:          return tr("Bookmark");
        case panicRecordingAction:    return tr("Panic recording");
        case sendMailAction:          return tr("Send email");
        case diagnosticsAction:       return tr("Write to log");
        case showPopupAction:         return tr("Show notification");
        case playSoundAction:         return tr("Repeat sound");
        case playSoundOnceAction:     return tr("Play sound");
        case sayTextAction:           return tr("Speak");
        case executePtzPresetAction:  return tr("Execute PTZ preset");
        case showTextOverlayAction:   return tr("Show text overlay");
        case showOnAlarmLayoutAction: return tr("Show on Alarm Layout");
        case execHttpRequestAction:   return tr("Do HTTP request");
        case acknowledgeAction:       return tr("Acknowledge");

        case cameraOutputAction:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Device output"),
                tr("Camera output"));

        case cameraRecordingAction:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Device recording"),
                tr("Camera recording"));

        default:
            break;
    }

    NX_ASSERT(false, Q_FUNC_INFO, "All enumeration values must be handled here");
    return lit("Unknown (%1)").arg(static_cast<int>(value));
}

QString StringsHelper::eventName(EventType value, int count) const
{
    if (value >= userDefinedEvent)
    {
        QString result = tr("Generic Event");
        if (value > userDefinedEvent)
            result += lit(" (%1)").arg((int)value - (int)userDefinedEvent); // reserved for future use
        return result;
    }

    switch (value)
    {
        case cameraMotionEvent:    return tr("Motion on Cameras", "", count);
        case storageFailureEvent:  return tr("Storage Failure");
        case networkIssueEvent:    return tr("Network Issue");
        case serverFailureEvent:   return tr("Server Failure");
        case serverConflictEvent:  return tr("Server Conflict");
        case serverStartEvent:     return tr("Server Started");
        case licenseIssueEvent:    return tr("License Issue");
        case backupFinishedEvent:  return tr("Archive backup finished");
        case analyticsSdkEvent:    return tr("Analytics Event");

        case anyServerEvent:       return tr("Any Server Issue");
        case anyEvent:             return tr("Any Event");

        case softwareTriggerEvent: return tr("Soft Trigger");

        case cameraInputEvent:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                    tr("Input Signal on Devices", "", count),
                    tr("Input Signal on Cameras", "", count));

        case cameraDisconnectEvent:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Devices Disconnected", "", count),
                tr("Cameras Disconnected", "", count));

        case cameraIpConflictEvent:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Devices IP Conflict", "", count),
                tr("Cameras IP Conflict", "", count));

        case anyCameraEvent:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Any Device Issue"),
                tr("Any Camera Issue"));

        default:
            return QString();
    }
}

QString StringsHelper::eventAtResource(const EventParameters& params,
    Qn::ResourceInfoLevel detailLevel) const
{
    const auto source = eventSource(params);
    const auto camera = source.dynamicCast<QnVirtualCameraResource>();
    const auto resourceName = QnResourceDisplayInfo(source).toString(detailLevel);

    switch (params.eventType)
    {
        case undefinedEvent:
            return tr("Undefined event has occurred on %1").arg(resourceName);

        case cameraDisconnectEvent:
            return QnDeviceDependentStrings::getNameFromSet(resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Device %1 was disconnected"),
                    tr("Camera %1 was disconnected"),
                    tr("I/O Module %1 was disconnected")), camera).arg(resourceName);

        case cameraInputEvent:
            return tr("Input on %1").arg(resourceName);

        case cameraMotionEvent:
            return tr("Motion on %1").arg(resourceName);

        case storageFailureEvent:
            return tr("Storage Failure at %1").arg(resourceName);

        case networkIssueEvent:
            return tr("Network Issue at %1").arg(resourceName);

        case serverFailureEvent:
            return tr("Server \"%1\" Failure").arg(resourceName);

        case cameraIpConflictEvent:
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("Device IP Conflict at %1", "Device IP Conflict at <server_name>"),
                tr("Camera IP Conflict at %1", "Camera IP Conflict at <server_name>"))
                .arg(resourceName);

        case serverConflictEvent:
            return tr("Server \"%1\" Conflict").arg(resourceName);

        case serverStartEvent:
            return tr("Server \"%1\" Started").arg(resourceName);

        case licenseIssueEvent:
            return tr("Server \"%1\" has a license problem").arg(resourceName);

        case backupFinishedEvent:
            return tr("Server \"%1\" has finished an archive backup").arg(resourceName);

        case userDefinedEvent:
        {
            if (!params.caption.isEmpty())
                return params.caption;

            return params.resourceName.isEmpty()
                ? tr("Generic Event")
                : tr("Generic Event at %1").arg(params.resourceName);
        }

        case softwareTriggerEvent:
            return tr("Soft Trigger %1 at %2")
                .arg(getSoftwareTriggerName(params))
                .arg(resourceName);

        case analyticsSdkEvent:
            return tr("%1 at %2", "Analytics Event at some camera")
                .arg(getAnalyticsSdkEventName(params))
                .arg(resourceName);

        default:
            return tr("An unknown event has occurred");
    }
}

QString StringsHelper::eventAtResources(const EventParameters& params) const
{
    if (params.eventType == softwareTriggerEvent)
    {
        return tr("Soft Trigger %1 has been activated multiple times")
            .arg(getSoftwareTriggerName(params));
    }

    return tr("Multiple %1 events have occured").arg(eventName(params.eventType));
}

QString StringsHelper::getResoureNameFromParams(const EventParameters& params,
    Qn::ResourceInfoLevel detailLevel) const
{
    QString result = QnResourceDisplayInfo(eventSource(params)).toString(detailLevel);
    return result.isEmpty() ? params.resourceName : result;
}

QString StringsHelper::getResoureIPFromParams(
    const EventParameters& params) const
{
	QString result = QnResourceDisplayInfo(eventSource(params)).host();
	return result.isNull() ? params.resourceName : result;
}

QStringList StringsHelper::eventDescription(const AbstractActionPtr& action,
    const AggregationInfo& aggregationInfo,
    Qn::ResourceInfoLevel detailLevel) const
{
    QStringList result;

    EventParameters params = action->getRuntimeParams();
    EventType eventType = params.eventType;

    result << tr("Event: %1").arg(eventName(eventType));

    QString sourceText = getResoureNameFromParams(params, detailLevel);
    if (!sourceText.isEmpty())
        result << tr("Source: %1").arg(sourceText);

    if (eventType >= userDefinedEvent || eventType == analyticsSdkEvent)
    {
        if (!params.caption.isEmpty())
            result << tr("Caption: %1").arg(params.caption);
    }

    const auto details = aggregatedEventDetails(action, aggregationInfo);
    result << details;

    return result;
}

QStringList StringsHelper::eventDetailsWithTimestamp(
    const EventParameters& params, int aggregationCount) const
{
    return QStringList()
        << eventTimestamp(params, aggregationCount)
        << eventDetails(params);
}

QStringList StringsHelper::eventDetails(const EventParameters& params) const
{
    QStringList result;
    switch (params.eventType)
    {
        case cameraInputEvent:
        {
            result << tr("Input Port: %1").arg(params.inputPortId);
            break;
        }

        case storageFailureEvent:
        case networkIssueEvent:
        case serverFailureEvent:
        case licenseIssueEvent:
        case backupFinishedEvent:
        {
            result << tr("Reason: %1").arg(eventReason(params));
            break;
        }

        case cameraIpConflictEvent:
        {
            result << tr("Conflicting Address: %1").arg(params.caption);
            int n = 0;
            for (const auto& mac: params.description.split(IpConflictEvent::delimiter()))
                result << tr("MAC #%1: %2").arg(++n).arg(mac);

            break;
        }

        case serverConflictEvent:
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

        case serverStartEvent:
            break;

        case analyticsSdkEvent:
        case userDefinedEvent:
            if (!params.description.isEmpty())
                result << params.description;
            break;

        case softwareTriggerEvent:
            result << tr("Trigger: %1").arg(getSoftwareTriggerName(params));
            break;

        default:
            break;
    }

    return result;
}

QString StringsHelper::eventTimestampShort(const EventParameters &params,
    int aggregationCount) const
{
    const auto ts = params.eventTimestampUsec;
    const auto time = QDateTime::fromMSecsSinceEpoch(ts/1000);

    const int count = qMax(aggregationCount, 1);
    return count == 1
        ? tr("%2 <b>%1</b>", "%1 means time, %2 means date")
            .arg(time.time().toString(Qt::DefaultLocaleShortDate))
            .arg(time.date().toString(Qt::DefaultLocaleShortDate))
        : tr("%n times, first: %2 <b>%1</b>", "%1 means time, %2 means date", count)
            .arg(time.time().toString(Qt::DefaultLocaleShortDate))
            .arg(time.date().toString(Qt::DefaultLocaleShortDate));
}

QString StringsHelper::eventTimestamp(const EventParameters &params,
    int aggregationCount) const
{
    const auto ts = params.eventTimestampUsec;
    const auto time = QDateTime::fromMSecsSinceEpoch(ts/1000);

    const int count = qMax(aggregationCount, 1);
    return count == 1
        ? tr("Time: %1 on %2", "%1 means time, %2 means date")
            .arg(time.time().toString(Qt::DefaultLocaleShortDate))
            .arg(time.date().toString(Qt::DefaultLocaleShortDate))
        : tr("First occurrence: %1 on %2 (%n times total)", "%1 means time, %2 means date", count)
            .arg(time.time().toString(Qt::DefaultLocaleShortDate))
            .arg(time.date().toString(Qt::DefaultLocaleShortDate));
}

QString StringsHelper::eventTimestampDate(const EventParameters &params) const
{
	quint64 ts = params.eventTimestampUsec;
	QDateTime time = QDateTime::fromMSecsSinceEpoch(ts / 1000);
	return time.date().toString(Qt::DefaultLocaleShortDate);
}

QString StringsHelper::eventTimestampTime(const EventParameters &params) const
{
	quint64 ts = params.eventTimestampUsec;
	QDateTime time = QDateTime::fromMSecsSinceEpoch(ts / 1000);
	return time.time().toString(Qt::DefaultLocaleShortDate);
}

QnResourcePtr StringsHelper::eventSource(const EventParameters &params) const
{
    QnUuid id = params.eventResourceId;
    return !id.isNull()
        ? resourcePool()->getResourceById(id)
        : QnResourcePtr();
}

QString StringsHelper::eventReason(const EventParameters& params) const
{
    QString reasonParamsEncoded = params.description;

    QString result;
    switch (params.reasonCode)
    {
        case EventReason::networkNoFrame:
        {
            int msecs = NetworkIssueEvent::decodeTimeoutMsecs(reasonParamsEncoded, 5000);
            result = tr("No data received during last %n seconds.", "", msecs / 1000);
            break;
        }
        case EventReason::networkConnectionClosed:
        {
            bool isPrimaryStream = NetworkIssueEvent::decodePrimaryStream(reasonParamsEncoded, true);

            QnVirtualCameraResourcePtr camera = eventSource(params).dynamicCast<QnVirtualCameraResource>();
            if (camera && !camera->hasVideo(nullptr))
                result = tr("Connection to device was unexpectedly closed.");
            else if (isPrimaryStream)
                result = tr("Connection to camera (primary stream) was unexpectedly closed.");
            else
                result = tr("Connection to camera (secondary stream) was unexpectedly closed.");
            break;
        }
        case EventReason::networkRtpPacketLoss:
        {
            NetworkIssueEvent::PacketLossSequence seq = NetworkIssueEvent::decodePacketLossSequence(reasonParamsEncoded);
            if (seq.valid)
                result = tr("RTP packet loss detected, prev seq.=%1 next seq.=%2.").arg(seq.prev).arg(seq.next);
            else
                result = tr("RTP packet loss detected.");
            break;
        }
        case EventReason::networkNoResponseFromDevice:
        {
            return tr("Device does not respond to network requests.");
        }
        case EventReason::serverTerminated:
        {
            result = tr("Connection to server is lost.");
            break;
        }
        case EventReason::serverStarted:
        {
            result = tr("Server stopped unexpectedly.");
            break;
        }
        case EventReason::storageIoError:
        {
            QString storageUrl = reasonParamsEncoded;
            result = tr("I/O error has occurred at %1.").arg(storageUrl);
            break;
        }
        case EventReason::storageTooSlow:
        {
            QString storageUrl = reasonParamsEncoded;
            result = tr("Not enough HDD/SSD speed for recording to %1.").arg(storageUrl);
            break;
        }
        case EventReason::storageFull:
        {
            QString storageUrl = reasonParamsEncoded;
            result = tr("HDD/SSD disk \"%1\" is full. Disk contains too much data that is not managed by VMS.").arg(storageUrl);
            break;
        }
        case EventReason::systemStorageFull:
        {
            QString storageUrl = reasonParamsEncoded;
            result = tr("System disk \"%1\" is almost full.").arg(storageUrl);
            break;
        }
        case EventReason::backupFailedNoBackupStorageError:
        {
            result = tr("Archive backup failed: No available backup storages with sufficient free space");
            break;
        }
        case EventReason::backupFailedSourceStorageError:
        {
            result = tr("Archive backup failed: Target storage failure");
            break;
        }
        case EventReason::backupFailedSourceFileError:
        {
            result = tr("Archive backup failed: Source file open/read error");
            break;
        }
        case EventReason::backupFailedTargetFileError:
        {
            result = tr("Archive backup failed: Target file create/write error");
            break;
        }
        case EventReason::backupFailedChunkError:
        {
            result = tr("Archive backup failed: File catalog error");
            break;
        }
        case EventReason::backupEndOfPeriod:
        {
            qint64 timeStampMs = params.description.toLongLong();
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(timeStampMs);
            // todo: #gdm add server/client timezone conversion
            result = tr("Archive backup finished, but is not fully completed because backup time is over. Data is backed up to %1").arg(dt.toString(Qt::DefaultLocaleShortDate));
        }
        case EventReason::backupDone:
        {
            result = tr("Archive backup is successfully completed");
            break;
        }
        case EventReason::backupCancelled:
        {
            qint64 timeStampMs = params.description.toLongLong();
            QDateTime dt = QDateTime::fromMSecsSinceEpoch(timeStampMs);
            // todo: #gdm add server/client timezone conversion
            result = tr("Archive backup is canceled by user. Data is backed up to %1").arg(dt.toString(Qt::DefaultLocaleShortDate));
            break;
        }
        case EventReason::licenseRemoved:
        {
            QnVirtualCameraResourceList disabledCameras;
            for (const auto& id: reasonParamsEncoded.split(L';'))
            {
                if (const auto& camera = resourcePool()->getResourceById<QnVirtualCameraResource>(QnUuid(id)))
                    disabledCameras << camera;
            }

            NX_ASSERT(!disabledCameras.isEmpty(), Q_FUNC_INFO, "At least one camera should be disabled on this event");

            result = QnDeviceDependentStrings::getNameFromSet(
                resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Not enough licenses. Recording has been disabled on following devices:"),
                    tr("Not enough licenses. Recording has been disabled on following cameras:"),
                    tr("Not enough licenses. Recording has been disabled on following I/O modules:")),
                disabledCameras);

            break;
        }

        default:
            break;
    }

    return result;
}

QStringList StringsHelper::aggregatedEventDetails(
    const AbstractActionPtr& action,
    const AggregationInfo& aggregationInfo) const
{
    QStringList result;
    if (aggregationInfo.isEmpty())
    {
        result << eventDetailsWithTimestamp(action->getRuntimeParams(),
            action->getAggregationCount());
    }

    for (const InfoDetail& detail: aggregationInfo.toList())
        result << eventDetailsWithTimestamp(detail.runtimeParams(), detail.count());

    return result;
}

QString StringsHelper::urlForCamera(const QnUuid& id, qint64 timestampUsec, bool isPublic) const
{
    if (id.isNull())
        return QString();

    const auto camera = resourcePool()->getResourceById<QnVirtualCameraResource>(id);
    if (!camera)
        return QString();

    auto server = camera->getParentServer();
    if (!server)
        return QString();

    quint64 timeStampMs = timestampUsec / 1000;
    QnMediaServerResourcePtr newServer = cameraHistoryPool()->getMediaServerOnTime(camera, timeStampMs);
    if (newServer)
        server = newServer;

    if (const auto& connection = camera->commonModule()->ec2Connection())
    {
        auto appServerUrl = connection->connectionInfo().ecUrl;
        if (appServerUrl.host().isEmpty() || resolveAddress(appServerUrl.host()) == QHostAddress::LocalHost)
        {
            appServerUrl = server->getApiUrl();
            if (isPublic)
            {
                const auto publicIP = server->getProperty(Qn::PUBLIC_IP);
                if (!publicIP.isEmpty())
                {
                    QStringList parts = publicIP.split(L':');
                    appServerUrl.setHost(parts[0]);
                    if (parts.size() > 1)
                        appServerUrl.setPort(parts[1].toInt());
                }
            }
        }

        QString result(lit("http://%1:%2/static/index.html#/view/%3?time=%4"));
        result = result.arg(appServerUrl.host()).arg(appServerUrl.port(80))
            .arg(camera->getId().toSimpleString()).arg(timeStampMs);
        return result;
    }

    return QString();
}

QString StringsHelper::toggleStateToString(EventState state) const
{
    switch (state)
    {
        case EventState::active:
            return tr("start");
        case EventState::inactive:
            return tr("stop");
        case EventState::undefined:
            return QString();
        default:
            return QString();
    }
}

QString StringsHelper::eventTypeString(
        EventType eventType,
        EventState eventState,
        ActionType actionType,
        const ActionParameters& actionParams) const
{
    QString typeStr = StringsHelper::eventName(eventType);
    if (isActionProlonged(actionType, actionParams))
        return tr("While %1").arg(typeStr);
    else
        return tr("On %1 %2").arg(typeStr).arg(toggleStateToString(eventState));
}

QString StringsHelper::ruleDescriptionText(const RulePtr& rule) const
{
    QString eventString = eventTypeString(
        rule->eventType(),
        rule->eventState(),
        rule->actionType(),
        rule->actionParams());

    return lit("%1 --> %2").arg(eventString).arg(
        actionName(rule->actionType()));
}

QString StringsHelper::defaultSoftwareTriggerName()
{
    return tr("Trigger Name");
}

QString StringsHelper::getSoftwareTriggerName(const QString& id)
{
    const auto triggerId = id.trimmed();
    return triggerId.isEmpty() ? defaultSoftwareTriggerName() : triggerId;
}

QString StringsHelper::getSoftwareTriggerName(const EventParameters& params)
{
    NX_ASSERT(params.eventType == softwareTriggerEvent);
    return getSoftwareTriggerName(params.caption);
}

QString StringsHelper::getAnalyticsSdkEventName(const EventParameters& params,
    const QString& locale) const
{
    NX_ASSERT(params.eventType == analyticsSdkEvent);

    QnUuid driverId = params.analyticsDriverId();
    NX_EXPECT(!driverId.isNull());

    QnUuid eventTypeId = params.analyticsEventId();
    NX_EXPECT(!eventTypeId.isNull());

    const auto source = eventSource(params);
    const auto camera = source.dynamicCast<QnVirtualCameraResource>();

    const auto eventType = analyticsEventType(camera, driverId, eventTypeId);
    const auto text = eventType.name.text(locale);

    return !text.isEmpty()
        ? text
        : tr("Analytics Event");
}

QString StringsHelper::actionSubjects(const RulePtr& rule, bool showName) const
{
    if (rule->actionParams().allUsers)
        return allUsersText();

    QnUserResourceList users;
    QList<QnUuid> roles;

    if (requiresUserResource(rule->actionType()))
        userRolesManager()->usersAndRoles(rule->actionResources(), users, roles);
    else
        userRolesManager()->usersAndRoles(rule->actionParams().additionalResources, users, roles);

    users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });
    return actionSubjects(users, roles, showName);
}

QString StringsHelper::actionSubjects(
    const QnUserResourceList& users,
    const QList<QnUuid>& roles,
    bool showName) const
{
    if (users.empty() && roles.empty())
        return needToSelectUserText();

    if (showName)
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

QString StringsHelper::allUsersText()
{
    return tr("All Users");
}

QString StringsHelper::needToSelectUserText()
{
    return tr("Select at least one user");
}

} // namespace event
} // namespace vms
} // namespace nx
