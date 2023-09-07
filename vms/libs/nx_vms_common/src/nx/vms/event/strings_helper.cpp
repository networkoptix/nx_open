// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "strings_helper.h"

#include <analytics/common/object_metadata.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/fusion/serialization/json.h>
#include <nx/network/url/url_builder.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/string.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/api/analytics/engine_manifest.h>
#include <nx/vms/common/html/html.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/time/formatter.h>
#include <nx_ec/abstract_ec_connection.h>
#include <utils/common/id.h>

#include "aggregation_info.h"
#include "events/events.h"
#include "rule.h"

using namespace nx::vms::api;

namespace {

QString poeConsumptionValue(double value)
{
    return QString::number(value, 'f', 1);
}

static const int kMaxValuesInGroup = 2;

} // namespace

using nx::analytics::taxonomy::AbstractState;
using nx::analytics::taxonomy::AbstractEngine;
using nx::analytics::taxonomy::AbstractEventType;
using nx::analytics::taxonomy::AbstractObjectType;

namespace nx {
namespace vms {
namespace event {

QString serializeAttributes(
    const nx::common::metadata::GroupedAttributes& attributes,
    const QString& delimiter,
    const QString& nestedAttributeDelimiter)
{
    QString result;
    for (const auto& group: attributes)
    {
        if (!result.isEmpty())
            result += delimiter;

        QString groupName = group.name;
        groupName.replace(".", nestedAttributeDelimiter);

        const auto count = std::min<qsizetype>(group.values.size(), kMaxValuesInGroup);
        result += NX_FMT("%1: %2",
            groupName,
            nx::utils::strJoin(group.values.cbegin(), group.values.cbegin() + count, ", "));
    }

    return result;
}

QStringList serializeAttributesMultiline(
    const nx::common::metadata::GroupedAttributes& attributes,
    const QString& nestedAttributeDelimiter)
{
    QStringList result;
    for (const auto& group: attributes)
    {
        QString groupName = group.name;
        groupName.replace(".", nestedAttributeDelimiter);

        const auto count = std::min<qsizetype>(group.values.size(), kMaxValuesInGroup);
        result << NX_FMT("%1: %2",
            groupName,
            nx::utils::strJoin(group.values.cbegin(), group.values.cbegin() + count, ", "));
    }

    return result;
}

StringsHelper::StringsHelper(common::SystemContext* context):
    QObject(),
    common::SystemContextAware(context)
{
}

QString StringsHelper::actionName(ActionType value) const
{
    switch (value)
    {
        case ActionType::undefinedAction:         return QString();
        case ActionType::bookmarkAction:          return tr("Bookmark");
        case ActionType::panicRecordingAction:    return tr("Panic recording");
        case ActionType::sendMailAction:          return tr("Send email");
        case ActionType::diagnosticsAction:       return tr("Write to log");
        case ActionType::showPopupAction:         return tr("Show desktop notification");
        case ActionType::pushNotificationAction:  return tr("Send mobile notification");
        case ActionType::playSoundAction:         return tr("Repeat sound");
        case ActionType::playSoundOnceAction:     return tr("Play sound");
        case ActionType::sayTextAction:           return tr("Speak");
        case ActionType::executePtzPresetAction:  return tr("Execute PTZ preset");
        case ActionType::showTextOverlayAction:   return tr("Show text overlay");
        case ActionType::showOnAlarmLayoutAction: return tr("Show on Alarm Layout");
        case ActionType::execHttpRequestAction:   return tr("Do HTTP(S) request");
        case ActionType::acknowledgeAction:       return tr("Acknowledge");
        case ActionType::openLayoutAction:        return tr("Open layout");
        case ActionType::fullscreenCameraAction:  return tr("Set to fullscreen");
        case ActionType::exitFullscreenAction:    return tr("Exit fullscreen");
        case ActionType::buzzerAction:            return tr("Buzzer");
        case ActionType::showIntercomInformer:    return tr("Show Intercom Informer");

        case ActionType::cameraOutputAction:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Device output"),
                tr("Camera output"));

        case ActionType::cameraRecordingAction:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Device recording"),
                tr("Camera recording"));
        default:
            break;
    }

    NX_ASSERT(false, "All enumeration values must be handled here");
    return lit("Unknown (%1)").arg(static_cast<int>(value));
}

QString StringsHelper::eventName(EventType value, int count) const
{
    if (value >= EventType::userDefinedEvent)
    {
        QString result = tr("Generic Event");
        if (value > EventType::userDefinedEvent)
            result += lit(" (%1)").arg((int)value - (int)EventType::userDefinedEvent); // reserved for future use
        return result;
    }

    switch (value)
    {
        case EventType::cameraMotionEvent:      return tr("Motion on Cameras", "", count);
        case EventType::storageFailureEvent:    return tr("Storage Issue");
        case EventType::networkIssueEvent:      return tr("Network Issue");
        case EventType::serverFailureEvent:     return tr("Server Failure");
        case EventType::serverConflictEvent:    return tr("Server Conflict");
        case EventType::serverStartEvent:       return tr("Server Started");
        case EventType::licenseIssueEvent:      return tr("License Issue");
        case EventType::backupFinishedEvent:    return tr("Archive Backup Finished");
        case EventType::analyticsSdkEvent:      return tr("Analytics Event");
        case EventType::analyticsSdkObjectDetected: return tr("Analytics Object Detected");
        case EventType::pluginDiagnosticEvent:  return tr("Plugin Diagnostic Event");
        case EventType::poeOverBudgetEvent:     return tr("PoE over Budget");
        case EventType::fanErrorEvent:          return tr("Fan Error");
        case EventType::serverCertificateError: return tr("Server Certificate Error");

        case EventType::anyServerEvent:         return tr("Any Server Issue");
        case EventType::anyEvent:               return tr("Any Event");

        case EventType::softwareTriggerEvent:   return tr("Soft Trigger");

        case EventType::cameraInputEvent:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                    tr("Input Signal on Devices", "", count),
                    tr("Input Signal on Cameras", "", count));

        case EventType::cameraDisconnectEvent:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Devices Disconnected", "", count),
                tr("Cameras Disconnected", "", count));

        case EventType::cameraIpConflictEvent:
            return QnDeviceDependentStrings::getDefaultNameFromSet(
                resourcePool(),
                tr("Devices IP Conflict", "", count),
                tr("Cameras IP Conflict", "", count));

        case EventType::anyCameraEvent:
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
        case EventType::undefinedEvent:
            return tr("Undefined event has occurred on %1").arg(resourceName);

        case EventType::cameraDisconnectEvent:
            return QnDeviceDependentStrings::getNameFromSet(resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Device %1 was disconnected"),
                    tr("Camera %1 was disconnected"),
                    tr("I/O Module %1 was disconnected")), camera).arg(resourceName);

        case EventType::cameraInputEvent:
            return tr("Input on %1").arg(resourceName);

        case EventType::cameraMotionEvent:
            return tr("Motion on %1").arg(resourceName);

        case EventType::storageFailureEvent:
            return tr("Storage Issue at %1").arg(resourceName);

        case EventType::networkIssueEvent:
            return tr("Network Issue at %1").arg(resourceName);

        case EventType::serverFailureEvent:
            return tr("Server \"%1\" Failure").arg(resourceName);

        case EventType::cameraIpConflictEvent:
            return QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(),
                tr("Device IP Conflict at %1", "Device IP Conflict at <server_name>"),
                tr("Camera IP Conflict at %1", "Camera IP Conflict at <server_name>"))
                .arg(resourceName);

        case EventType::serverConflictEvent:
            return tr("Server \"%1\" Conflict").arg(resourceName);

        case EventType::serverStartEvent:
            return tr("Server \"%1\" Started").arg(resourceName);

        case EventType::licenseIssueEvent:
            return tr("Server \"%1\" has a license problem").arg(resourceName);

        case EventType::backupFinishedEvent:
            return tr("Server \"%1\" has finished an archive backup").arg(resourceName);

        case EventType::userDefinedEvent:
        {
            if (!params.caption.isEmpty())
                return params.caption;

            return params.resourceName.isEmpty()
                ? tr("Generic Event")
                : tr("Generic Event at %1").arg(params.resourceName);
        }

        case EventType::softwareTriggerEvent:
            return tr("Soft Trigger %1 at %2")
                .arg(getSoftwareTriggerName(params))
                .arg(resourceName);

        case EventType::analyticsSdkEvent:
        {
            const auto eventName = getAnalyticsSdkEventName(params);
            NX_ASSERT(!eventName.isEmpty());
            return tr("%1 at %2", "Analytics Event at some camera")
                .arg(params.caption.isEmpty() ? eventName : params.caption)
                .arg(resourceName);
        }
        case EventType::analyticsSdkObjectDetected:
        {
            const auto eventName = getAnalyticsSdkObjectName(params);
            NX_ASSERT(!eventName.isEmpty());
            // for example: "Car at camera 'Front' is detected"
            return tr("%1 at camera '%2'", " is detected")
                .arg(eventName)
                .arg(resourceName);
        }
        case EventType::pluginDiagnosticEvent:
        {
            const QString caption = params.caption.isEmpty()
                ? tr("Unknown Plugin Diagnostic Event")
                : params.caption;

            return nx::format("%1 - %2").args(resourceName, caption);
        }
        case EventType::poeOverBudgetEvent:
        {
            return tr("PoE over budget at %1").arg(resourceName);
        }
        case EventType::fanErrorEvent:
        {
            return tr("Fan error at %1").arg(resourceName);
        }
        case EventType::serverCertificateError:
        {
            return tr("Server \"%1\" certificate error").arg(
                resourceName.isEmpty() ? params.eventResourceId.toSimpleString() : resourceName);
        }
        default:
            return tr("An unknown event has occurred");
    }
}

QString StringsHelper::eventAtResources(const EventParameters& params) const
{
    if (params.eventType == EventType::softwareTriggerEvent)
    {
        return tr("Soft Trigger %1 has been activated multiple times")
            .arg(getSoftwareTriggerName(params));
    }

    return tr("Multiple %1 events have occurred").arg(eventName(params.eventType));
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
    Qn::ResourceInfoLevel detailLevel,
    AttrSerializePolicy policy) const
{
    QStringList result;

    EventParameters params = action->getRuntimeParams();
    EventType eventType = params.eventType;

    result << tr("Event: %1").arg(eventName(eventType));

    QString sourceText = getResoureNameFromParams(params, detailLevel);
    if (!sourceText.isEmpty())
        result << tr("Source: %1").arg(sourceText);

    if (!params.analyticsEngineId.isNull())
    {
        const std::shared_ptr<AbstractState> taxonomyState = m_context->analyticsTaxonomyState();
        if (taxonomyState)
        {
            const AbstractEngine* engineInfo =
                taxonomyState->engineById(params.analyticsEngineId.toString());

            if (engineInfo)
                result << tr("Plugin: %1").arg(engineInfo->name());
        }
    }

    if (eventType >= EventType::userDefinedEvent || eventType == EventType::analyticsSdkEvent)
    {
        if (!params.caption.isEmpty() && !params.description.startsWith(params.caption))
            result << tr("Caption: %1").arg(params.caption);
    }

    if (eventType == EventType::poeOverBudgetEvent)
    {
        const auto consumption = poeConsumptionStringFromParams(params);
        if (!consumption.isEmpty())
            result << tr("Reason: Power limit exceeded (%1)", "%1 is consumption").arg(consumption);
    }

    const auto details = aggregatedEventDetails(action, aggregationInfo, policy);
    result << details;

    return result;
}

QStringList StringsHelper::eventDetailsWithTimestamp(
    const EventParameters& params, int aggregationCount, AttrSerializePolicy policy) const
{
    return QStringList()
        << eventTimestamp(params, aggregationCount)
        << eventDetails(params, policy);
}

QStringList StringsHelper::eventDetails(
    const EventParameters& params, AttrSerializePolicy policy) const
{
    QStringList result;
    switch (params.eventType)
    {
        case EventType::cameraInputEvent:
        {
            result << tr("Input Port: %1").arg(params.inputPortId);
            break;
        }

        case EventType::storageFailureEvent:
        case EventType::networkIssueEvent:
        case EventType::serverFailureEvent:
        case EventType::licenseIssueEvent:
        case EventType::backupFinishedEvent:
        {
            result << tr("Reason: %1").arg(eventReason(params));
            break;
        }

        case EventType::cameraIpConflictEvent:
        {
            result << tr("Conflicting Address: %1").arg(params.caption);
            int n = 0;
            for (const auto& mac: IpConflictEvent::decodeMacAddrList(params))
                result << tr("MAC #%1: %2").arg(++n).arg(mac);

            break;
        }

        case EventType::serverConflictEvent:
        {
            if (!params.description.isEmpty())
            {
                nx::vms::rules::CameraConflictList conflicts;
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

        case EventType::serverStartEvent:
            break;

        case EventType::userDefinedEvent:
            if (!params.description.isEmpty())
                result << params.description;
            break;
        case EventType::analyticsSdkEvent:
        case EventType::pluginDiagnosticEvent:
        case EventType::analyticsSdkObjectDetected:
        {
            QString message;

            if (!params.caption.isEmpty())
                message = params.caption;
            if (!params.description.isEmpty())
            {
                if (!message.isEmpty())
                    message += ": ";
                message += params.description;
            }

            if (params.attributes.empty() || policy == AttrSerializePolicy::none)
            {
                if (!message.isEmpty())
                    result << message;
                break;
            }

            if (policy == AttrSerializePolicy::singleLine)
            {
                if (!message.isEmpty())
                    message += ". ";
                message += serializeAttributes(
                    nx::common::metadata::groupAttributes(params.attributes),
                    /* delimiter */ "; ",
                    /* nestedAttributeDelimiter */ " ");
                if (!message.isEmpty())
                    result << message;
            }
            else
            {
                if (!message.isEmpty())
                    result << message;
                result << serializeAttributesMultiline(
                    nx::common::metadata::groupAttributes(params.attributes),
                    /* nestedAttributeDelimiter */ " ");
            }

            break;
        }
        case EventType::softwareTriggerEvent:
            result << tr("Trigger: %1").arg(getSoftwareTriggerName(params));
            break;

        default:
            break;
    }

    return result;
}

QString StringsHelper::eventTimestampInHtml(const EventParameters &params,
    int aggregationCount) const
{
    const auto ts = params.eventTimestampUsec;
    const auto time = QDateTime::fromMSecsSinceEpoch(ts/1000);

    const int count = qMax(aggregationCount, 1);
    return count == 1
        ? tr("%2 <b>%1</b>", "%1 means time, %2 means date")
            .arg(nx::vms::time::toString(time.time()))
            .arg(nx::vms::time::toString(time.date()))
        : tr("%n times, first: %2 <b>%1</b>", "%1 means time, %2 means date", count)
            .arg(nx::vms::time::toString(time.time()))
            .arg(nx::vms::time::toString(time.date()));
}

QString StringsHelper::eventTimestamp(const EventParameters &params,
    int aggregationCount) const
{
    const auto ts = params.eventTimestampUsec;
    const auto time = QDateTime::fromMSecsSinceEpoch(ts/1000);

    const int count = qMax(aggregationCount, 1);
    return count == 1
        ? tr("Time: %1 on %2", "%1 means time, %2 means date")
            .arg(nx::vms::time::toString(time.time()))
            .arg(nx::vms::time::toString(time.date()))
        : tr("First occurrence: %1 on %2 (%n times total)", "%1 means time, %2 means date", count)
            .arg(nx::vms::time::toString(time.time()))
            .arg(nx::vms::time::toString(time.date()));
}

QString StringsHelper::eventTimestampDate(const EventParameters &params) const
{
    quint64 ts = params.eventTimestampUsec;
    QDateTime time = QDateTime::fromMSecsSinceEpoch(ts / 1000);
    return nx::vms::time::toString(time.date());
}

QString StringsHelper::eventTimestampTime(const EventParameters &params) const
{
    quint64 ts = params.eventTimestampUsec;
    QDateTime time = QDateTime::fromMSecsSinceEpoch(ts / 1000);
    return nx::vms::time::toString(time.time());
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
            int msecs = NetworkIssueEvent::decodeTimeoutMsecs(reasonParamsEncoded);
            result = tr("No data received during last %n seconds.", "", msecs / 1000);
            break;
        }
        case EventReason::networkRtpStreamError:
        {
            QString message;
            bool isPrimaryStream = NetworkIssueEvent::decodePrimaryStream(
                reasonParamsEncoded, &message);
            result = isPrimaryStream
                ? tr("RTP error in primary stream (%1).").arg(message)
                : tr("RTP error in secondary stream (%1).").arg(message);
            break;
        }
        case EventReason::networkConnectionClosed:
        {
            const bool isPrimaryStream =
                NetworkIssueEvent::decodePrimaryStream(reasonParamsEncoded);
            const auto camera = eventSource(params).dynamicCast<QnVirtualCameraResource>();

            const auto deviceType = camera
                ? QnDeviceDependentStrings::calculateDeviceType(resourcePool(), {camera})
                : QnCameraDeviceType::Mixed;

            if (deviceType == QnCameraDeviceType::Camera)
            {
                return isPrimaryStream
                    ? tr("Connection to camera (primary stream) was unexpectedly closed.")
                    : tr("Connection to camera (secondary stream) was unexpectedly closed.");
            }
            else
            {
                return tr("Connection to device was unexpectedly closed.");
            }
        }
        case EventReason::networkRtpPacketLoss:
        {
            return tr("RTP packet loss detected.");
        }
        case EventReason::networkBadCameraTime:
        {
            return tr("Failed to force using camera time, as it lags too much."
                " System time will be used instead.");
        }
        case EventReason::networkCameraTimeBackToNormal:
        {
            return tr("Camera time is back to normal.");
        }
        case EventReason::networkNoResponseFromDevice:
        {
            return tr("Device does not respond to network requests.");
        }
        case EventReason::networkMulticastAddressConflict:
        {
            const auto params =
                QJson::deserialized<NetworkIssueEvent::MulticastAddressConflictParameters>(
                    reasonParamsEncoded.toUtf8());

            const auto addressLine = (params.stream == StreamIndex::primary)
                ? tr("Address %1 is already in use by %2 on primary stream.",
                    "%1 is the address, %2 is the device name")
                : tr("Address %1 is already in use by %2 on secondary stream.",
                    "%1 is the address, %2 is the device name");

            return tr("Multicast address conflict detected.") + " "
                + addressLine
                    .arg(QString::fromStdString(params.address.toString()))
                    .arg(params.deviceName);
        }
        case EventReason::networkMulticastAddressIsInvalid:
        {
            const auto params = QJson::deserialized<nx::network::SocketAddress>(
                reasonParamsEncoded.toUtf8());

            return tr("Network address %1 is not a multicast address.")
                .arg(params.toString().c_str());
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
        case EventReason::metadataStorageOffline:
        {
            QString storageUrl = reasonParamsEncoded;
            result = tr("Analytics storage \"%1\" is offline.").arg(storageUrl);
            break;
        }
        case EventReason::metadataStorageFull:
        {
            QString storageUrl = reasonParamsEncoded;
            result = tr("Analytics storage \"%1\" is almost full.").arg(storageUrl);
            break;
        }
        case EventReason::metadataStoragePermissionDenied:
        {
            QString storageUrl = reasonParamsEncoded;
            result = tr("Analytics storage \"%1\" database error: Insufficient permissions on the "
                "mount point.").arg(storageUrl);
            break;
        }
        case EventReason::encryptionFailed:
        {
            result = tr("Cannot initialize AES encryption while recording is enabled on the media "
                "archive. Data is written unencrypted.");
            break;
        }
        case EventReason::raidStorageError:
        {
            result = tr("RAID error: %1.").arg(reasonParamsEncoded);
            break;
        }
        case EventReason::backupFailedSourceFileError:
        {
            result = tr("Archive backup failed.") + " " + backupResultText(params);
            break;
        }
        case EventReason::licenseRemoved:
        {
            const auto idList = nx::vms::event::LicenseIssueEvent::decodeCameras(params);
            NX_ASSERT(!idList.isEmpty(), "At least one camera should be disabled on this event");

            result = QnDeviceDependentStrings::getNameFromSet(
                resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Not enough licenses. Recording has been disabled on the following devices:"),
                    tr("Not enough licenses. Recording has been disabled on the following cameras:"),
                    tr("Not enough licenses. Recording has been disabled on the following I/O modules:")),
                resourcePool()->getResourcesByIds<QnVirtualCameraResource>(idList));

            break;
        }

        default:
            break;
    }

    return result;
}

QStringList StringsHelper::aggregatedEventDetails(
    const AbstractActionPtr& action,
    const AggregationInfo& aggregationInfo,
    AttrSerializePolicy policy) const
{
    QStringList result;
    if (aggregationInfo.isEmpty())
    {
        result << eventDetailsWithTimestamp(action->getRuntimeParams(),
            action->getAggregationCount(), policy);
    }

    for (const InfoDetail& detail: aggregationInfo.toList())
        result << eventDetailsWithTimestamp(detail.runtimeParams(), detail.count(), policy);

    return result;
}

QString StringsHelper::urlForCamera(
    const QnUuid& id,
    std::chrono::milliseconds timestamp,
    bool usePublicIp,
    const std::optional<nx::network::SocketAddress>& proxyAddress) const
{
    static constexpr int kDefaultHttpPort = 80;

    if (id.isNull())
        return QString();

    nx::network::url::Builder builder;
    builder.setScheme(nx::network::http::kSecureUrlSchemeName);
    if (proxyAddress) //< Client-side method to form camera links in the event log.
    {
        builder.setEndpoint(*proxyAddress);
    }
    else //< Server-side method to form links in emails.
    {
        const auto camera =
            resourcePool()->getResourceById<QnVirtualCameraResource>(id);
        if (!camera)
            return QString();

        auto server = camera->getParentServer();
        if (!server)
            return QString();

        auto newServer =
            m_context->cameraHistoryPool()->getMediaServerOnTime(camera, timestamp.count());
        if (newServer)
            server = newServer;

        nx::utils::Url serverUrl = server->getRemoteUrl();
        if (usePublicIp)
        {
            const auto publicIp = server->getProperty(ResourcePropertyKey::Server::kPublicIp);
            if (publicIp.isEmpty())
                return QString();

            const QStringList parts = publicIp.split(':');
            builder.setHost(parts[0]);
            if (parts.size() > 1)
                builder.setPort(parts[1].toInt());
            else
                builder.setPort(serverUrl.port(kDefaultHttpPort));
        }
        else /*usePublicIp*/
        {
            builder.setHost(serverUrl.host());
            builder.setPort(serverUrl.port(kDefaultHttpPort));
        }
    }

    builder.setPath("/static/index.html")
        .setFragment("/view/" + id.toSimpleString())
        .setQuery(QString("time=%1").arg(timestamp.count()));

    const nx::utils::Url url = builder.toUrl();
    NX_ASSERT(url.isValid());
    return url.toWebClientStandardViolatingUrl();
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

QString StringsHelper::sourceCameraCheckboxText(ActionType actionType) const
{
    switch (actionType)
    {
        case ActionType::showOnAlarmLayoutAction:
            return tr("Also show source camera");

        case ActionType::fullscreenCameraAction:
            return tr("Source camera");

        case ActionType::showTextOverlayAction:
            return tr("Also show on source camera");

        case ActionType::bookmarkAction:
            return tr("Also set on source camera");

        case ActionType::cameraOutputAction:
            return tr("Also trigger on source camera");

        case ActionType::cameraRecordingAction:
            return tr("Also record source camera");

        case ActionType::playSoundOnceAction:
        case ActionType::playSoundAction:
        case ActionType::sayTextAction:
            return tr("Also play on source camera");

        default:
            NX_ASSERT("Action can't use source camera: %1", actionType);
            return "Use source camera";
    }
}

QString StringsHelper::defaultSoftwareTriggerName()
{
    return tr("Trigger Name");
}

QString StringsHelper::getSoftwareTriggerName(const QString& name)
{
    const auto triggerId = name.trimmed();
    return triggerId.isEmpty() ? defaultSoftwareTriggerName() : triggerId;
}

QString StringsHelper::getSoftwareTriggerName(const EventParameters& params)
{
    NX_ASSERT(params.eventType == EventType::softwareTriggerEvent);
    return getSoftwareTriggerName(params.caption);
}

QString StringsHelper::getAnalyticsSdkEventName(const EventParameters& params,
    const QString& /*locale*/) const
{
    NX_ASSERT(params.eventType == EventType::analyticsSdkEvent);

    const QString eventTypeId = params.getAnalyticsEventTypeId();
    NX_ASSERT(!eventTypeId.isEmpty());

    const auto source = eventSource(params);
    const auto camera = source.dynamicCast<QnVirtualCameraResource>();

    if (const std::shared_ptr<AbstractState> taxonomyState = m_context->analyticsTaxonomyState())
    {
        if (const AbstractEventType* eventType = taxonomyState->eventTypeById(eventTypeId);
            camera && eventType)
        {
            return eventType->name();
        }
    }

    return tr("Analytics Event");
}

QString StringsHelper::getAnalyticsSdkObjectName(const EventParameters& params,
    const QString& /*locale*/) const
{
    NX_ASSERT(params.eventType == EventType::analyticsSdkObjectDetected);

    const QString objectTypeId = params.getAnalyticsObjectTypeId();
    NX_ASSERT(!objectTypeId.isEmpty());

    const auto source = eventSource(params);
    const auto camera = source.dynamicCast<QnVirtualCameraResource>();

    std::shared_ptr<AbstractState> taxonomyState;
    if (camera && camera->systemContext())
        taxonomyState = camera->systemContext()->analyticsTaxonomyState();
    const auto objectType = taxonomyState
        ? taxonomyState->objectTypeById(objectTypeId)
        : nullptr;

    return objectType ? objectType->name() : tr("Object detected");
}

QString StringsHelper::actionSubjects(const RulePtr& rule, bool showName) const
{
    if (rule->actionParams().allUsers)
        return allUsersText();

    QnUserResourceList users;
    UserGroupDataList groups;

    if (requiresUserResource(rule->actionType()))
    {
        nx::vms::common::getUsersAndGroups(m_context,
            rule->actionResources(), users, groups);
    }
    else
    {
        nx::vms::common::getUsersAndGroups(m_context,
            rule->actionParams().additionalResources, users, groups);
    }

    users = users.filtered([](const QnUserResourcePtr& user) { return user->isEnabled(); });
    return actionSubjects(users, groups, showName);
}

QString StringsHelper::actionSubjects(
    const QnUserResourceList& users,
    const UserGroupDataList& groups,
    bool showName) const
{
    if (users.empty() && groups.empty())
        return needToSelectUserText();

    if (showName)
    {
        if (users.size() == 1 && groups.empty())
            return users.front()->getName();

        if (users.empty() && groups.size() <= 2)
        {
            QStringList groupNames;
            for (const auto& group: groups)
                groupNames.push_back(std::move(group.name));
            groupNames.sort(Qt::CaseInsensitive);

            return groupNames.join(", ");
        }
    }

    if (groups.empty())
        return tr("%n Users", "", users.size());

    if (!users.empty())
    {
        return lit("%1, %2")
            .arg(tr("%n Groups", "", groups.size()))
            .arg(tr("%n Users", "", users.size()));
    }

    return tr("%n Groups", "", groups.size());
}

QString StringsHelper::allUsersText()
{
    return tr("All Users");
}

QString StringsHelper::needToSelectUserText()
{
    return tr("Select at least one user");
}

QString StringsHelper::poeConsumption()
{
    return tr("Consumption");
}

QString StringsHelper::poeConsumptionString(double current, double limit)
{
    return qFuzzyIsNull(current) || current < 0
        ? QString("0 W")
        : poeOverallConsumptionString(current, limit);
}

QString StringsHelper::poeOverallConsumptionString(double current, double limit)
{
    return QString("%1 W / %2 W").arg(poeConsumptionValue(current)).arg(poeConsumptionValue(limit));
}

QString StringsHelper::poeConsumptionStringFromParams(const EventParameters& params)
{
    const auto values = nx::vms::event::PoeOverBudgetEvent::consumptionParameters(params);
    if (values.isEmpty())
        return QString();

    return poeOverallConsumptionString(values.currentConsumptionWatts, values.upperLimitWatts);
}

QString StringsHelper::backupResultText(const EventParameters& params)
{
    switch (params.reasonCode)
    {
        case EventReason::backupFailedSourceFileError:
            return tr("Failed to backup file %1").arg(params.description);

        default:
            NX_ASSERT(false);
            return {};
    }
}

QString StringsHelper::backupTimeText(const QDateTime& time)
{
    return tr("Data is backed up to %1").arg(nx::vms::time::toString(time));
}

QString StringsHelper::notificationCaption(
    const EventParameters& parameters,
    const QnVirtualCameraResourcePtr& camera,
    bool useHtml) const
{
    switch (parameters.eventType)
    {
        case EventType::softwareTriggerEvent:
        {
            const auto event = eventName(parameters.eventType);
            const auto trigger = getSoftwareTriggerName(parameters);
            return QString("%1 %2")
                .arg(event, useHtml ? nx::vms::common::html::bold(trigger) : trigger);
        }

        case EventType::userDefinedEvent:
            return parameters.caption.isEmpty()
                ? tr("Generic Event")
                : parameters.caption;

        case EventType::pluginDiagnosticEvent:
            return parameters.caption.isEmpty()
                ? tr("Unknown Plugin Diagnostic Event")
                : parameters.caption;

        case EventType::analyticsSdkEvent:
            return parameters.caption.isEmpty()
                ? getAnalyticsSdkEventName(parameters)
                : parameters.caption;
        case EventType::analyticsSdkObjectDetected:
            return parameters.caption.isEmpty()
                ? getAnalyticsSdkObjectName(parameters)
                : parameters.caption;

        case EventType::cameraDisconnectEvent:
            return QnDeviceDependentStrings::getNameFromSet(resourcePool(),
                QnCameraDeviceStringSet(
                    tr("Device was disconnected"),
                    tr("Camera was disconnected"),
                    tr("I/O Module was disconnected")), camera);

        case EventType::backupFinishedEvent:
            return tr("Archive backup failed");

        default:
            return eventName(parameters.eventType);
    }
}

QString StringsHelper::notificationDescription(const EventParameters& parameters) const
{
    switch (parameters.eventType)
    {
        case EventType::userDefinedEvent:
        case EventType::pluginDiagnosticEvent:
        case EventType::analyticsSdkEvent:
        case EventType::analyticsSdkObjectDetected:
            return parameters.description;

        case EventType::poeOverBudgetEvent:
        {
            // Note: this code is used to generate mobile push notifications only.
            // It desktop client it is overridden by the one with rich text formatting.
            const auto consumptionString = poeConsumptionStringFromParams(parameters);
            if (consumptionString.isEmpty())
                return QString();

            return QString("%1 %2")
                .arg(poeConsumption())
                .arg(consumptionString);
        }

        default:
            return QString();
    }
}

} // namespace event
} // namespace vms
} // namespace nx
