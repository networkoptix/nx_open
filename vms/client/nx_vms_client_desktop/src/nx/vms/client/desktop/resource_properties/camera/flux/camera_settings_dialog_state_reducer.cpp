// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_settings_dialog_state_reducer.h"

#include <chrono>
#include <limits>

#include <QtCore/QUrlQuery>
#include <QtNetwork/QAuthenticator>

#include <camera/fps_calculator.h>
#include <client/client_module.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_media_stream_info.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <core/resource/resource_display_info.h>
#include <core/resource/resource_media_layout.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/utils.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/json/serializer.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/algorithm/same.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_manager.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/utils/camera_hotspots_support.h>
#include <utils/camera/camera_bitrate_calculator.h>

#include "../camera_advanced_parameters_manifest_manager.h"
#include "../utils/device_agent_settings_adapter.h"
#include "../watchers/camera_settings_analytics_engines_watcher.h"

namespace nx::vms::client::desktop {

using namespace std::chrono;

using State = CameraSettingsDialogState;
using RecordingThresholds = State::RecordingSettings::Thresholds;
using Rotation = core::Rotation;
using StandardRotation = core::StandardRotation;

using UsingOnvifMedia2Type = nx::core::resource::UsingOnvifMedia2Type;

using Camera = QnVirtualCameraResourcePtr;
using Cameras = QnVirtualCameraResourceList;

using MotionType = nx::vms::api::MotionType;
using StreamIndex = nx::vms::api::StreamIndex;
using RecordingType = nx::vms::api::RecordingType;
using RecordingMetadataType = nx::vms::api::RecordingMetadataType;
using RecordingMetadataTypes = nx::vms::api::RecordingMetadataTypes;

using namespace std::chrono;

namespace {

static constexpr int kMinFps = 1;

static constexpr auto kMinQuality = Qn::StreamQuality::low;
static constexpr auto kMaxQuality = Qn::StreamQuality::highest;

static constexpr auto kMinArchiveDurationAlertThreshold = std::chrono::days(5);
static constexpr auto kPreRecordingAlertThreshold = 30s;

QnIOPortDataList sortedPorts(QnIOPortDataList ports)
{
    std::sort(ports.begin(), ports.end(),
        [](const QnIOPortData& left, const QnIOPortData& right)
        {
            return nx::utils::naturalStringCompare(left.id, right.id, Qt::CaseInsensitive) < 0;
        });
    return ports;
}

template<class Data>
void fetchFromCameras(
    UserEditableMultiple<Data>& value,
    const Cameras& cameras,
    std::function<Data(const Camera&)> getter)
{
    Data data{};
    value.resetBase();
    if (!cameras.isEmpty() &&
        nx::utils::algorithm::same(cameras.cbegin(), cameras.cend(), getter, &data))
    {
        value.setBase(data);
    }
}

template<class Data, class Intermediate>
void fetchFromCameras(
    UserEditableMultiple<Data>& value,
    const Cameras& cameras,
    std::function<Intermediate(const Camera&)> getter,
    std::function<Data(const Intermediate&)> converter)
{
    Intermediate data{};
    value.resetBase();
    if (!cameras.isEmpty() &&
        nx::utils::algorithm::same(cameras.cbegin(), cameras.cend(), getter, &data))
    {
        value.setBase(converter(data));
    }
}

CombinedValue combinedValue(const Cameras& cameras, std::function<bool(const Camera&)> predicate)
{
    bool value = false;
    if (cameras.isEmpty()
        || !nx::utils::algorithm::same(cameras.cbegin(), cameras.cend(), predicate, &value))
    {
        return CombinedValue::Some;
    }

    return value ? CombinedValue::All : CombinedValue::None;
}

/** Get camera webpage address as http(s)://host[:port] */
nx::utils::Url getBaseCameraUrl(const Camera& camera)
{
    if (!NX_ASSERT(camera))
        return {};

    auto url = nx::utils::Url::fromUserInput(camera->getUrl());
    if (!url.isValid())
        url.setHost(camera->getHostAddress());

    const auto scheme = url.scheme().toStdString();

    // Avoid using a port that accepts non-HTTP(S) protocol.
    if (!scheme.empty() && !nx::network::http::isUrlScheme(scheme))
        url.setPort(-1);

    // Force HTTP or HTTPS protocol for a webpage.
    // TODO: #sivanov Update protocol on global settings change.
    const bool useSecureScheme = camera->systemContext()->globalSettings()->useHttpsOnlyForCameras()
        || (nx::utils::stricmp(scheme, nx::network::http::kSecureUrlSchemeName) == 0);

    url.setScheme(nx::network::http::urlScheme(useSecureScheme));

    // Port number may be passed in query parameter "http_port".
    const QUrlQuery query(url.query());
    const int port =
        query.queryItemValue(QnVirtualCameraResource::kHttpPortParameterName).toInt();
    if (port > 0)
        url.setPort(port);

    return url;
}

bool isMotionDetectionEnabled(const Camera& camera)
{
    return camera->isMotionDetectionEnabled();
}

bool hasMotion(const Camera& camera)
{
    return camera->getDefaultMotionType() != MotionType::none
        && camera->getStatus() != nx::vms::api::ResourceStatus::unauthorized;
}

bool calculateRecordingParametersAvailable(const Cameras& cameras)
{
    return std::any_of(cameras.cbegin(), cameras.cend(),
        [](const Camera& camera) { return camera->isVideoQualityAdjustable(); });
}

float calculateBitrateForQualityMbps(const State& state, Qn::StreamQuality streamQuality)
{
    return common::CameraBitrateCalculator::roundKbpsToMbps(
        common::CameraBitrateCalculator::suggestBitrateForQualityKbps(
            streamQuality,
            state.recording.defaultStreamResolution,
            state.recording.brush.fps,
            QString(), //< Calculate bitrate for default codec.
            state.recording.mediaStreamCapability,
            state.recording.useBitratePerGop));
}

Qn::StreamQuality calculateQualityForBitrateMbps(const State& state, float bitrateMbps)
{
    auto current = kMinQuality;
    auto currentBr = calculateBitrateForQualityMbps(state, current);

    for (int i = (int) current + 1; i <= (int) kMaxQuality; ++i)
    {
        const auto next = Qn::StreamQuality(i);
        const auto nextBr = calculateBitrateForQualityMbps(state, next);

        if (bitrateMbps < (currentBr + nextBr) * 0.5)
            break;

        currentBr = nextBr;
        current = next;
    }

    return current;
}

State loadMinMaxCustomBitrate(State state)
{
    State::RecordingSettings& settings = state.recording;
    settings.minBitrateMbps = calculateBitrateForQualityMbps(state, Qn::StreamQuality::lowest);
    settings.maxBitrateMpbs = calculateBitrateForQualityMbps(state, Qn::StreamQuality::highest);

    static const std::array<Qn::StreamQuality, 4> kUserVisibleQualities{{
        Qn::StreamQuality::low,
        Qn::StreamQuality::normal,
        Qn::StreamQuality::high,
        Qn::StreamQuality::highest}};

    settings.minRelevantQuality = Qn::StreamQuality::lowest;
    for (const auto streamQuality: kUserVisibleQualities)
    {
        if (calculateBitrateForQualityMbps(state, streamQuality) > settings.minBitrateMbps)
            break;

        settings.minRelevantQuality = streamQuality;
    }

    // Brush stream quality may not be lower than minimal.
    settings.brush.streamQuality =
        std::max(settings.brush.streamQuality, settings.minRelevantQuality);

    return state;
}

State fillBitrateFromFixedQuality(State state)
{
    state.recording.brush.bitrateMbitPerSec = RecordScheduleCellData::kAutoBitrateValue;
    state.recording.bitrateMbps = calculateBitrateForQualityMbps(state,
        state.recording.brush.streamQuality);

    return state;
}

QString settingsUrlPath(const Camera& camera)
{
    const auto resourceData = camera->resourceData();
    if (!resourceData.value<bool>(lit("showUrl"), false))
        return QString();

    QString urlPath = resourceData.value<QString>(lit("urlLocalePath"), QString());
    while (urlPath.startsWith(lit("/"))) //< VMS Gateway does not like slashes at the beginning.
        urlPath = urlPath.mid(1);

    return urlPath;
}

State loadStreamUrls(State state, const Camera& camera)
{
    state.singleCameraSettings.primaryStream.setBase(
        camera->sourceUrl(Qn::CR_LiveVideo));
    state.singleCameraSettings.secondaryStream.setBase(
        camera->sourceUrl(Qn::CR_SecondaryLiveVideo));
    return state;
}

State updateCameraWebPage(State state)
{
    auto url = state.singleCameraProperties.baseCameraUrl;

    // Omit default port number for selected scheme.
    if (nx::network::http::defaultPortForScheme(url.scheme().toStdString()) == url.port())
        url.setPort(-1);

    if (state.expert.customWebPagePort.valueOr(0) > 0)
        url.setPort(state.expert.customWebPagePort());

    const auto schemeHostPort = url.toString(QUrl::RemovePath | QUrl::RemoveQuery);
    state.singleCameraProperties.webPageLabelText =
        nx::format("<a href=\"%1\">%1</a>", schemeHostPort);
    state.singleCameraProperties.settingsUrl = schemeHostPort + "/" +
        state.singleCameraProperties.settingsUrlPath;

    return state;
}

State loadNetworkInfo(State state, const Camera& camera)
{
    NX_ASSERT(camera);
    if (!camera)
        return state;

    state.singleCameraProperties.baseCameraUrl = getBaseCameraUrl(camera);
    state.singleCameraProperties.settingsUrlPath = settingsUrlPath(camera);

    state = updateCameraWebPage(std::move(state));

    state.singleCameraProperties.ipAddress = QnResourceDisplayInfo(camera).host();
    state.singleCameraProperties.overrideXmlHttpRequestTimeout =
        camera->resourceData().value<int>("overrideXmlHttpRequestTimeout", 0);
    state.singleCameraProperties.overrideHttpUserAgent =
        camera->resourceData().value<QString>("overrideHttpUserAgent");
    state.singleCameraProperties.fixupRequestUrls =
        camera->resourceData().value<bool>("fixupRequestUrls", true); //< Enabled by default.
    state = loadStreamUrls(std::move(state), camera);

    return state;
}

State::ImageControlSettings calculateImageControlSettings(
    const Cameras& cameras)
{
    const bool hasVideo = std::all_of(cameras.cbegin(), cameras.cend(),
        [](const auto& camera) { return camera->hasVideo(); });

    State::ImageControlSettings result;
    result.aspectRatioAvailable = hasVideo && std::all_of(cameras.cbegin(), cameras.cend(),
        [](const auto& camera) { return !camera->hasFlags(Qn::virtual_camera); });

    if (result.aspectRatioAvailable)
    {
        fetchFromCameras<QnAspectRatio>(
            result.aspectRatio,
            cameras,
            [](const auto& camera) { return camera->customAspectRatio(); });
    }

    result.rotationAvailable = hasVideo;

    if (result.rotationAvailable)
    {
        fetchFromCameras<StandardRotation>(
            result.rotation,
            cameras,
            [](const auto& camera)
            {
                const auto rotation = camera->forcedRotation();
                return rotation.has_value()
                    ? Rotation::standardRotation(*rotation)
                    : StandardRotation::rotate0;
            });
    }

    return result;
}

QnScheduleTaskList calculateRecordingSchedule(const Camera& camera)
{
    if (camera->isDtsBased())
        return {};

    auto schedule = camera->getScheduleTasks();
    int maxFps = camera->getMaxFps();

    // TODO: Code duplication with
    // `CameraSettingsDialogStateConversionFunctions::applyStateToCameras`.
    if (schedule.empty())
    {
        if (camera->getStatus() == nx::vms::api::ResourceStatus::unauthorized)
            maxFps = QnSecurityCamResource::kDefaultMaxFps;

        schedule = nx::vms::common::defaultSchedule(maxFps);
    }
    else if (camera->isOnline()) //< Fix FPS if we perfectly sure it is calculated correctly.
    {
        for (auto& task: schedule)
            task.fps = std::min((int)task.fps, maxFps);
    }

    return schedule;
}

int calculateRecordingThresholdBefore(const Camera& camera)
{
    const auto value = camera->recordBeforeMotionSec();
    return value > 0 ? value : vms::api::kDefaultRecordBeforeMotionSec;
}

int calculateRecordingThresholdAfter(const Camera& camera)
{
    const auto value = camera->recordAfterMotionSec();
    return value > 0 ? value : vms::api::kDefaultRecordAfterMotionSec;
}

QnMotionRegion::ErrorCode validateMotionRegionList(
    const State& state,
    const QList<QnMotionRegion>& regionList)
{
    if (!state.motion.constraints)
        return QnMotionRegion::ErrorCode::Ok;

    for (const auto& region: regionList)
    {
        const auto errorCode = region.isValid(
            state.motion.constraints->maxTotalRects,
            state.motion.constraints->maxMaskRects,
            state.motion.constraints->maxSensitiveRects);

        if (errorCode != QnMotionRegion::ErrorCode::Ok)
            return errorCode;
    }

    return QnMotionRegion::ErrorCode::Ok;
}

QList<QnMotionRegion> calculateMotionRegionList(const State& state, const Camera& camera)
{
    auto regionList = camera->getMotionRegionList();
    const int channelCount = camera->getVideoLayout()->channelCount();

    if (regionList.size() > channelCount)
        regionList.erase(regionList.begin() + channelCount, regionList.end());
    else while (regionList.size() < channelCount)
        regionList << QnMotionRegion();

    if (validateMotionRegionList(state, regionList) != QnMotionRegion::ErrorCode::Ok)
    {
        for (auto& region: regionList)
            region = QnMotionRegion(); //< Reset to default.
    }

    return regionList;
}

State updateDuplicateLogicalIdInfo(State state)
{
    state.singleCameraSettings.sameLogicalIdCameraNames.clear();

    const auto currentId(state.singleCameraProperties.id);
    const auto currentLogicalId = state.singleCameraSettings.logicalId();

    const auto isSameLogicalId =
        [&currentId, &currentLogicalId](const Camera& camera)
        {
            return camera->logicalId() == currentLogicalId
                && camera->getId() != currentId;
        };

    if (currentLogicalId > 0)
    {
        const auto duplicateCameras = qnClientCoreModule->resourcePool()->
            getAllCameras().filtered(isSameLogicalId);

        for (const auto& camera: duplicateCameras)
            state.singleCameraSettings.sameLogicalIdCameraNames << camera->getName();
    }

    return state;
}

bool isDefaultExpertSettings(const State& state)
{
    if (state.isSingleCamera() && state.singleCameraSettings.logicalId() > 0)
        return false;

    if (state.settingsOptimizationEnabled)
    {
        if ((state.devicesDescription.isArecontCamera == CombinedValue::None
            && state.expert.cameraControlDisabled.valueOr(true))
            || state.expert.useBitratePerGOP.valueOr(true)
            || state.expert.dualStreamingDisabled.valueOr(true))
        {
            return false;
        }

        if (!state.expert.keepCameraTimeSettings.valueOr(false))
            return false;
    }

    if (!state.expert.useMedia2ToFetchProfiles.equals(UsingOnvifMedia2Type::autoSelect))
        return false;

    if (state.expert.primaryRecordingDisabled.valueOr(true)
        || (state.devicesDescription.hasDualStreamingCapability != CombinedValue::None
            && state.expert.secondaryRecordingDisabled.valueOr(true)))
    {
        return false;
    }

    if (!state.expert.recordAudioEnabled.valueOr(false))
        return false;

    if (state.canSwitchPtzPresetTypes() && (!state.expert.preferredPtzPresetType.hasValue()
        || state.expert.preferredPtzPresetType() != nx::core::ptz::PresetType::undefined))
    {
        return false;
    }

    if (state.canForcePanTiltCapabilities() &&
        state.expert.forcedPtzPanTiltCapability.valueOr(true))
    {
        return false;
    }

    if (state.canForcePanTiltCapabilities() && state.expert.forcedPtzZoomCapability.valueOr(true))
        return false;

    if (state.hasPanTiltCapabilities() && state.expert.doNotSendStopPtzCommand.valueOr(true))
        return false;

    if (!state.expert.customWebPagePort.equals(0))
        return false;

    if (state.devicesDescription.hasCustomMediaPortCapability == CombinedValue::All
        && (!state.expert.customMediaPort.hasValue() //< Some of cameras have custom media port.
            || state.expert.customMediaPort() != 0)) //< All cameras have custom media port.
    {
        return false;
    }

    if (state.expert.trustCameraTime.valueOr(true))
        return false;

    if (!state.expert.forcedPrimaryProfile.valueOr({}).isEmpty())
        return false;

    if (!state.expert.forcedSecondaryProfile.valueOr({}).isEmpty())
        return false;

    if (state.expert.remoteArchiveSyncronizationMode.valueOr(
        nx::vms::common::RemoteArchiveSyncronizationMode::off)
            != nx::vms::common::RemoteArchiveSyncronizationMode::off)
    {
        return false;
    }

    return state.expert.rtpTransportType.hasValue()
        && state.expert.rtpTransportType() == vms::api::RtpTransportType::automatic;
}

RecordingType fixupRecordingType(const State& state)
{
    auto result = state.recording.brush.recordingType;

    const bool hasDualStreaming = state.isDualStreamingEnabled();
    if (result == RecordingType::metadataAndLowQuality && !hasDualStreaming)
        result = RecordingType::metadataOnly;

    const bool hasMetadata = state.isMotionDetectionActive() || state.isObjectDetectionSupported();
    if (result == RecordingType::metadataOnly && !hasMetadata)
        result = RecordingType::always;

    return result;
}

bool isMetadataRadioButtonAllowed(const State& state, State::MetadataRadioButton value)
{
    switch (value)
    {
        case State::MetadataRadioButton::motion:
            return state.isMotionDetectionActive() || !state.isObjectDetectionSupported();
        case State::MetadataRadioButton::objects:
            return state.isObjectDetectionSupported();
        case State::MetadataRadioButton::both:
            return state.isMotionDetectionActive() && state.isObjectDetectionSupported();
        default:
            NX_ASSERT(false, "Unreachable");
            return false;
    }
}

State::MetadataRadioButton fixupMetadataRadioButton(const State& state)
{
    auto result = state.recording.metadataRadioButton;
    if (isMetadataRadioButtonAllowed(state, result))
        return result;

    const bool onlyObjectsAvailable = state.isObjectDetectionSupported()
        && !state.isMotionDetectionActive();
    return onlyObjectsAvailable
        ? State::MetadataRadioButton::objects
        : State::MetadataRadioButton::motion;
}

/** Fixup recording brush type and metadata types, fixup metadata radiobutton value. */
State fixupRecordingBrush(State state)
{
    state.recording.brush.recordingType = fixupRecordingType(state);
    state.recording.metadataRadioButton = fixupMetadataRadioButton(state);
    if (state.recording.brush.recordingType == RecordingType::metadataAndLowQuality
        || state.recording.brush.recordingType == RecordingType::metadataOnly)
    {
        state.recording.brush.metadataTypes = State::radioButtonToMetadataTypes(
            state.recording.metadataRadioButton);
    }
    else
    {
        state.recording.brush.metadataTypes = {};
    }
    return state;
}

/** Calculate if camera support object detection concerning planning changes on engines tab. */
CombinedValue calculateSingleCameraObjectDetectionSupport(const State& state)
{
    const bool hasObjects = !QnVirtualCameraResource::filterByEngineIds(
        state.singleCameraProperties.supportedAnalyicsObjectTypes,
        state.analytics.enabledEngines()).empty();
    return hasObjects
        ? CombinedValue::All
        : CombinedValue::None;
}

/** Calculate if all cameras support object detection. */
CombinedValue calculateObjectDetectionSupport(const State& state, const Cameras& cameras)
{
    if (state.isSingleCamera())
        return calculateSingleCameraObjectDetectionSupport(state);

    // In multi-camera dialog analytics engines info is not available and not editable.
    return combinedValue(cameras,
        [](const Camera& camera) { return !camera->supportedObjectTypes().empty(); });
}

bool isValidScheduleTask(const State& state, const QnScheduleTask& task)
{
    return QnVirtualCameraResource::canApplyScheduleTask(
        task,
        state.isDualStreamingEnabled(),
        state.isMotionDetectionActive(),
        state.isObjectDetectionSupported());
}

State::ScheduleAlerts calculateScheduleAlerts(const State& state)
{
    if (!state.supportsSchedule()
        || !state.recording.schedule.hasValue()
        || !state.recording.enabled.valueOr(false))
    {
        return {};
    }

    State::ScheduleAlerts scheduledAlerts;

    for (const auto& task: state.recording.schedule())
    {
        if (isValidScheduleTask(state, task))
            continue;

        const bool motion = task.metadataTypes.testFlag(RecordingMetadataType::motion);
        const bool objects = task.metadataTypes.testFlag(RecordingMetadataType::objects);

        switch (task.recordingType)
        {
            case RecordingType::metadataOnly:
            {
                if (motion && objects)
                    scheduledAlerts |= State::ScheduleAlert::noBoth;
                else if (motion)
                    scheduledAlerts |= State::ScheduleAlert::noMotion;
                else if (objects)
                    scheduledAlerts |= State::ScheduleAlert::noObjects;
                break;
            }
            case RecordingType::metadataAndLowQuality:
            {
                if (motion && objects)
                    scheduledAlerts |= State::ScheduleAlert::noBothLowRes;
                else if (motion)
                    scheduledAlerts |= State::ScheduleAlert::noMotionLowRes;
                else if (objects)
                    scheduledAlerts |= State::ScheduleAlert::noObjectsLowRes;
                break;
            }
            default:
                break;
        }
    }

    return scheduledAlerts;
}

void updateRecordingAlerts(State& state)
{
    const bool highArchiveLengthWarning = state.recording.minPeriod.hasManualPeriodValue()
        && state.recording.minPeriod.displayValue() > kMinArchiveDurationAlertThreshold;
    state.recordingAlerts.setFlag(
        State::RecordingAlert::highArchiveLength, highArchiveLengthWarning);

    const bool highPreRecordingValueWarning = state.recording.thresholds.beforeSec.hasValue()
        && seconds(state.recording.thresholds.beforeSec()) >= kPreRecordingAlertThreshold;
    state.recordingAlerts.setFlag(
        State::RecordingAlert::highPreRecordingValue, highPreRecordingValueWarning);
}

bool canForceZoomCapability(const Camera& camera)
{
    return camera->ptzCapabilitiesUserIsAllowedToModify()
        .testFlag(Ptz::Capability::ContinuousZoomCapability);
};

bool hasPanTiltCapabilities(const Camera& camera)
{
    return camera->getPtzCapabilities().testAnyFlags({
        Ptz::Capability::AbsolutePanTiltCapabilities,
        Ptz::Capability::ContinuousPanTiltCapabilities});
}

bool canSwitchPtzPresetTypes(const Camera& camera)
{
    return camera->canSwitchPtzPresetTypes();
}

bool canForcePanTiltCapabilities(const Camera& camera)
{
    return camera->ptzCapabilitiesUserIsAllowedToModify() &
        Ptz::Capability::ContinuousPanTiltCapabilities;
};

bool analyticsEngineIsPresentInList(const QnUuid& id, const State& state)
{
    const auto& engines = state.analytics.engines;
    return std::any_of(engines.cbegin(), engines.cend(),
        [&id](const auto& info) { return info.id == id; });
}

bool isCompletelyMaskedOut(const QList<QnMotionRegion>& regions)
{
    for (const auto& region: regions)
    {
        for (int i = 1; i < QnMotionRegion::kSensitivityLevelCount; ++i)
        {
            if (!region.getRegionBySens(i).isEmpty())
                return false;
        }
    }

    return true;
};

QSize motionStreamResolution(const State& state, ModificationSource source)
{
    const auto stream = source == ModificationSource::local
        ? state.effectiveMotionStream()
        : state.motion.stream.getBase();

    switch (stream)
    {
        case StreamIndex::primary:
            return state.singleCameraProperties.primaryStreamResolution;

        case StreamIndex::secondary:
            return state.singleCameraProperties.secondaryStreamResolution;

        default:
            NX_ASSERT(false);
            return {};
    }
}

bool isMotionResolutionTooHigh(const State& state,
    ModificationSource source = ModificationSource::local)
{
    if (!state.isSingleCamera() || !state.motion.supportsSoftwareDetection)
        return false;

    const auto resolution = motionStreamResolution(state, source);
    const int pixels = resolution.width() * resolution.height();
    return pixels > QnVirtualCameraResource::kMaximumMotionDetectionPixels;
}

bool isMotionImplicitlyDisabledOnServer(const State& state)
{
    return !state.motion.forced.getBase()
        && state.motion.enabled.getBase().value_or(false)
        && isMotionResolutionTooHigh(state, ModificationSource::remote);
}

std::optional<State::MotionStreamAlert> calculateMotionStreamAlert(const State& state)
{
    if (state.isSingleCamera())
    {
        if (!state.motion.enabled.equals(true) || !isMotionResolutionTooHigh(state))
            return {};

        if (isMotionImplicitlyDisabledOnServer(state) && !state.motion.forced())
            return State::MotionStreamAlert::implicitlyDisabled;

        return (state.effectiveMotionStream() != state.motion.stream() && !state.motion.forced())
            ? State::MotionStreamAlert::willBeImplicitlyDisabled
            : State::MotionStreamAlert::resolutionTooHigh;
    }
    else
    {
        if (state.motion.dependingOnDualStreaming != CombinedValue::None
            && state.expert.dualStreamingDisabled.equals(true)
            && !state.motion.forced())
        {
            return State::MotionStreamAlert::willBeImplicitlyDisabled;
        }

        return {};
    }
}

State handleStreamParametersChange(State state)
{
    state.motion.streamAlert = calculateMotionStreamAlert(state);
    state = fixupRecordingBrush(std::move(state));
    state.scheduleAlerts = calculateScheduleAlerts(state);
    return state;
}

State loadSingleCameraProperties(
    State state,
    const Camera& singleCamera,
    DeviceAgentSettingsAdapter* deviceAgentSettingsAdapter,
    CameraSettingsAnalyticsEnginesWatcherInterface* analyticsEnginesWatcher,
    CameraAdvancedParametersManifestManager* advancedParametersManifestManager)
{
    auto& singleProperties = state.singleCameraProperties;
    singleProperties.id = singleCamera->getId();
    singleProperties.name.setBase(singleCamera->getName());
    singleProperties.isOnline = singleCamera->isOnline();
    singleProperties.firmware = singleCamera->getFirmware();
    singleProperties.model = singleCamera->getModel();
    singleProperties.vendor = singleCamera->getVendor();
    singleProperties.hasVideo = singleCamera->hasVideo();
    singleProperties.editableStreamUrls = singleCamera->hasCameraCapabilities(
        nx::vms::api::DeviceCapability::customMediaUrl);
    singleProperties.networkLink = singleCamera->hasCameraCapabilities(
        nx::vms::api::DeviceCapabilities(nx::vms::api::DeviceCapability::customMediaUrl)
        | nx::vms::api::DeviceCapability::fixedQuality);

    singleProperties.primaryStreamResolution =
        singleCamera->streamInfo(StreamIndex::primary).getResolution();
    singleProperties.secondaryStreamResolution =
        singleCamera->streamInfo(StreamIndex::secondary).getResolution();

    singleProperties.supportedAnalyicsObjectTypes = singleCamera->supportedObjectTypes(
        /*filterByEngines*/ false);

    const auto macAddress = singleCamera->getMAC();
    singleProperties.macAddress = macAddress.isNull() ? QString() : macAddress.toString();

    state.motion.supportsSoftwareDetection = singleCamera->supportedMotionTypes().testFlag(
        MotionType::software);

    if (singleCamera->getDefaultMotionType() == MotionType::hardware)
    {
        state.motion.constraints = camera_settings_detail::MotionConstraints();
        state.motion.constraints->maxTotalRects = singleCamera->motionWindowCount();
        state.motion.constraints->maxMaskRects = singleCamera->motionMaskWindowCount();
        state.motion.constraints->maxSensitiveRects
            = singleCamera->motionSensWindowCount();
    }

    if (advancedParametersManifestManager)
    {
        singleProperties.advancedSettingsManifest =
            advancedParametersManifestManager->manifest(singleCamera);
    }

    const auto fisheyeParams = singleCamera->getDewarpingParams();
    state.singleCameraSettings.dewarpingParams.setBase(fisheyeParams);

    state = loadNetworkInfo(std::move(state), singleCamera);

    singleProperties.usbDevice = singleProperties.vendor == "usb_cam"
        && singleProperties.macAddress.isEmpty()
        && state.singleCameraSettings.primaryStream().isEmpty()
        && state.singleCameraSettings.secondaryStream().isEmpty();

    singleProperties.supportsCameraHotspots =
        nx::vms::common::camera_hotspots::supportsCameraHotspots(singleCamera);

    state.recording.defaultStreamResolution = singleCamera->streamInfo().getResolution();
    state.recording.mediaStreamCapability = singleCamera->cameraMediaCapability().
        streamCapabilities.value(StreamIndex::primary);

    state.recording.customBitrateAvailable = true;

    state.motion.enabled.setBase(singleCamera->isMotionDetectionEnabled());
    state.motion.regionList.setBase(calculateMotionRegionList(state, singleCamera));

    const auto motionStreamInfo = singleCamera->motionStreamIndex();
    state.motion.stream.setBase(motionStreamInfo.index);
    state.motion.forced.setBase(motionStreamInfo.isForced);

    if (singleCamera->isIOModule())
    {
        state.singleIoModuleSettings.visualStyle.setBase(
            nx::reflect::fromString<vms::api::IoModuleVisualStyle>(
                singleCamera->getProperty(ResourcePropertyKey::kIoOverlayStyle).toStdString(),
                {}));

        state.singleIoModuleSettings.ioPortsData.setBase(
            sortedPorts(singleCamera->ioPortDescriptions()));
    }

    state.singleCameraSettings.logicalId.setBase(singleCamera->logicalId());
    state = updateDuplicateLogicalIdInfo(std::move(state));

    if (analyticsEnginesWatcher)
    {
        bool changed = false;
        std::tie(changed, state) = CameraSettingsDialogStateReducer::setAnalyticsEngines(
            std::move(state),
            analyticsEnginesWatcher->engineInfoList());

        for (const auto& engine: state.analytics.engines)
        {
            state.analytics.streamByEngineId[engine.id].setBase(
                analyticsEnginesWatcher->analyzedStreamIndex(engine.id));
        }
    }

    if (deviceAgentSettingsAdapter)
    {
        for (auto [engineId, deviceAgentData]: deviceAgentSettingsAdapter->dataByEngineId())
        {
            state = CameraSettingsDialogStateReducer::resetDeviceAgentData(
                std::move(state),
                engineId,
                deviceAgentData).second;
        }
    }

    state.analytics.userEnabledEngines.setBase(singleCamera->userEnabledAnalyticsEngines());
    state.analytics.userEnabledEngines.resetUser();

    // Recalculate to take in account engines info.
    state.devicesDescription.hasObjectDetection =
        calculateSingleCameraObjectDetectionSupport(state);

    state.virtualCameraIgnoreTimeZone = singleCamera->virtualCameraIgnoreTimeZone();

    state.singleCameraSettings.audioInputDeviceId = singleCamera->audioInputDeviceId();
    state.singleCameraSettings.audioOutputDeviceId = singleCamera->audioOutputDeviceId();

    if (state.singleCameraProperties.supportsCameraHotspots)
    {
        state.singleCameraSettings.cameraHotspotsEnabled.setBase(
            singleCamera->cameraHotspotsEnabled());
        state.singleCameraSettings.cameraHotspots.setBase(singleCamera->cameraHotspots());
    }

    return state;
}

} // namespace

State CameraSettingsDialogStateReducer::setSelectedTab(State state, CameraSettingsTab value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.selectedTab = value;
    return state;
}

State CameraSettingsDialogStateReducer::setReadOnly(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.readOnly = value;
    return state;
}

State CameraSettingsDialogStateReducer::setSettingsOptimizationEnabled(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.settingsOptimizationEnabled = value;
    state = fixupRecordingBrush(std::move(state));
    state.scheduleAlerts = calculateScheduleAlerts(state);
    return state;
}

State CameraSettingsDialogStateReducer::setHasPowerUserPermissions(
    State state,
    bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.hasPowerUserPermissions = value;
    return state;
}

State CameraSettingsDialogStateReducer::setHasEventLogPermission(
    State state,
    bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.hasEventLogPermission = value;
    return state;
}

State CameraSettingsDialogStateReducer::setHasEditAccessRightsForAllCameras(
    State state,
    bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.hasEditAccessRightsForAllCameras = value;

    if (!state.hasEditAccessRightsForAllCameras)
    {
        state.singleCameraSettings.cameraHotspotsEnabled.resetUser();
        state.singleCameraSettings.cameraHotspots.resetUser();
    }

    return state;
}

CameraSettingsDialogStateReducer::State CameraSettingsDialogStateReducer::setPermissions(
    State state,
    Qn::Permissions value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.singleCameraProperties.permissions = value;

    return state;
}

CameraSettingsDialogStateReducer::State CameraSettingsDialogStateReducer::setAllCamerasEditable(
    State state,
    bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.allCamerasEditable = value;

    return state;
}

State CameraSettingsDialogStateReducer::setSaasInitialized(
    State state,
    bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.saasInitialized = value;

    return state;
}

State CameraSettingsDialogStateReducer::setSingleVirtualCameraState(
    State state, const VirtualCameraState& value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, nx::reflect::json::serialize(value));

    state.singleVirtualCameraState = value;
    state.virtualCameraUploaderName = QString();

    if (state.singleVirtualCameraState.status == VirtualCameraState::LockedByOtherClient)
    {
        if (const auto user = qnClientCoreModule->resourcePool()->getResourceById(
            state.singleVirtualCameraState.lockUserId))
        {
            state.virtualCameraUploaderName = user->getName();
        }
    }

    return state;
}

State CameraSettingsDialogStateReducer::updatePtzSettings(
    State state,
    const Cameras& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    state.devicesDescription.canSwitchPtzPresetTypes =
        combinedValue(cameras, canSwitchPtzPresetTypes);

    state.devicesDescription.canForcePanTiltCapabilities =
        combinedValue(cameras, canForcePanTiltCapabilities);

    state.devicesDescription.canForceZoomCapability =
        combinedValue(cameras, canForceZoomCapability);

    state.devicesDescription.hasPanTiltCapabilities =
        combinedValue(cameras, hasPanTiltCapabilities);

    if (state.canSwitchPtzPresetTypes())
    {
        fetchFromCameras<nx::core::ptz::PresetType>(state.expert.preferredPtzPresetType,
            cameras.filtered([](const Camera& camera) { return camera->canSwitchPtzPresetTypes(); }),
            [](const Camera& camera) { return camera->userPreferredPtzPresetType(); });
    }

    if (state.canForcePanTiltCapabilities())
    {
        fetchFromCameras<bool>(state.expert.forcedPtzPanTiltCapability, cameras,
            [](const Camera& camera)
            {
                return camera->ptzCapabilitiesAddedByUser().testFlag(
                    Ptz::ContinuousPanTiltCapabilities);
            });
    }

    if (state.canForceZoomCapability())
    {
        fetchFromCameras<bool>(state.expert.forcedPtzZoomCapability, cameras,
            [](const Camera& camera)
            {
                return camera->ptzCapabilitiesAddedByUser().testFlag(
                    Ptz::ContinuousZoomCapability);
            });
    }

    if (state.hasPanTiltCapabilities())
    {
        fetchFromCameras<bool>(state.expert.doNotSendStopPtzCommand, cameras,
            [](const Camera& camera)
            {
                const auto clientCamera = camera.dynamicCast<QnClientCameraResource>();
                return !(clientCamera && clientCamera->autoSendPtzStopCommand());
            });
    }

    const auto hasPtzSensitivity =
        [](const Camera& camera)
        {
            return camera->hasAnyOfPtzCapabilities(Ptz::ContinuousPanTiltCapabilities);
        };

    state.devicesDescription.canAdjustPtzSensitivity = combinedValue(cameras, hasPtzSensitivity);

    if (state.canAdjustPtzSensitivity())
    {
        const auto relevantCameras = cameras.filtered(hasPtzSensitivity);

        fetchFromCameras<qreal>(state.expert.ptzSensitivity.pan, relevantCameras,
            [](const Camera& camera) { return camera->ptzPanTiltSensitivity().x(); });

        fetchFromCameras<bool>(state.expert.ptzSensitivity.separate, relevantCameras,
            [](const Camera& camera) { return !camera->isPtzPanTiltSensitivityUniform(); });

        fetchFromCameras<qreal>(state.expert.ptzSensitivity.tilt, relevantCameras,
            [](const Camera& camera) { return camera->ptzPanTiltSensitivity().y(); });
    }

    return state;
}

nx::vms::api::DeviceProfiles intersectProfiles(
    const nx::vms::api::DeviceProfiles& left,
    const nx::vms::api::DeviceProfiles& right)
{
    nx::vms::api::DeviceProfiles result;
    for (const auto& p: left)
    {
        if (std::any_of(right.begin(), right.end(),
            [token = p.token](const auto& p)
            {
                return p.token == token;
            }))
        {
            result.push_back(p);
        }
    }
    return result;
}

State CameraSettingsDialogStateReducer::loadCameras(
    State state,
    const Cameras& cameras,
    DeviceAgentSettingsAdapter* deviceAgentSettingsAdapter,
    CameraSettingsAnalyticsEnginesWatcherInterface* analyticsEnginesWatcher,
    CameraAdvancedParametersManifestManager* advancedParametersManifestManager)
{
    NX_VERBOSE(NX_SCOPE_TAG, "Reset state, loading cameras %1", cameras);

    const auto singleCamera = (cameras.size() == 1)
        ? cameras.first()
        : Camera();

    // TODO: #vbreus Revisit it.
    if (!cameras.empty())
    {
        const auto allCameras = cameras.first()->resourcePool()->getAllCameras(
            QnResourcePtr(), /*ignoreDesktopCameras*/ true);

        state.systemHasDevicesWithAudioInput =
            std::any_of(std::cbegin(allCameras), std::cend(allCameras),
            [](const auto& camera)
            {
                return camera->isAudioSupported();
            });

        state.systemHasDevicesWithAudioOutput =
            std::any_of(std::cbegin(allCameras), std::cend(allCameras),
            [](const auto& camera)
            {
                return camera->hasTwoWayAudio();
            });
    }

    // TODO: #vkutin #sivanov Separate camera-dependent state from camera-independent state.
    // Reset camera-dependent state with a single call.
    state.hasChanges = false;
    state.singleCameraProperties = {};
    state.singleCameraSettings = {};
    state.singleIoModuleSettings = {};
    state.devicesDescription = {};
    state.credentials = {};
    state.expert = {};
    state.recording = {};
    state.virtualCameraMotion = {};
    state.devicesCount = cameras.size();
    state.audioEnabled = {};
    state.twoWayAudioEnabled = {};
    state.recordingHint = {};
    state.recordingAlerts = {};
    state.motionHint = {};
    state.motionAlert = {};
    state.expertAlerts = {};
    state.scheduleAlerts = {};
    state.analytics.userEnabledEngines = {};
    state.analytics.settingsByEngineId = {};
    state.analytics.streamByEngineId = {};
    state.motion = {};
    state.motion.currentSensitivity = QnMotionRegion::kDefaultSensitivity;
    state.virtualCameraIgnoreTimeZone = false;
    state.expert.availableProfiles.clear();

    state.deviceType = singleCamera
        ? QnDeviceDependentStrings::calculateDeviceType(singleCamera->resourcePool(), cameras)
        : QnCameraDeviceType::Mixed;

    state.devicesDescription.isDtsBased = combinedValue(cameras,
        [](const Camera& camera) { return camera->isDtsBased(); });
    state.devicesDescription.supportsSchedule = combinedValue(cameras,
        [](const Camera& camera) { return camera->supportsSchedule(); });

    state.devicesDescription.isVirtualCamera = combinedValue(cameras,
        [](const Camera& camera) { return camera->hasFlags(Qn::virtual_camera); });
    state.devicesDescription.isIoModule = combinedValue(cameras,
        [](const Camera& camera) { return camera->isIOModule(); });

    state.devicesDescription.supportsVideo = combinedValue(cameras,
        [](const Camera& camera) { return camera->hasVideo(); });
    state.devicesDescription.supportsAudio = combinedValue(cameras,
        [](const Camera& camera) { return camera->isAudioSupported(); });
    state.devicesDescription.supportsWebPage = combinedValue(cameras,
        [](const Camera& camera) { return camera->isWebPageSupported(); });
    state.devicesDescription.isAudioForced = combinedValue(cameras,
        [](const Camera& camera) { return camera->isAudioForced(); });
    state.devicesDescription.supportsAudioOutput = combinedValue(cameras,
        [](const Camera& camera)
        {
            return camera->hasTwoWayAudio();
        });

    state.devicesDescription.hasMotion = combinedValue(cameras, &hasMotion);

    state.devicesDescription.hasObjectDetection = calculateObjectDetectionSupport(
        state,
        cameras);

    state.devicesDescription.hasDualStreamingCapability = combinedValue(cameras,
        [](const Camera& camera) { return camera->hasDualStreamingInternal(); });

    state.devicesDescription.isUdpMulticastTransportAllowed = combinedValue(cameras,
        [](const Camera& camera)
        {
            return camera->hasCameraCapabilities(
                nx::vms::api::DeviceCapability::multicastStreaming);
        });

    state.devicesDescription.streamCapabilitiesInitialized = combinedValue(cameras,
        [](const Camera& camera)
        {
            return !camera->getProperty(ResourcePropertyKey::kMediaCapabilities).isEmpty();
        });

    state.devicesDescription.hasRemoteArchiveCapability = combinedValue(cameras,
        [](const Camera& camera)
        {
            return camera->hasCameraCapabilities(
                nx::vms::api::DeviceCapability::remoteArchive);
        });

    state.devicesDescription.isArecontCamera = combinedValue(cameras,
        [](const Camera& camera)
        {
            const auto cameraType = qnResTypePool->getResourceType(camera->getTypeId());
            return cameraType && cameraType->getManufacturer() == lit("ArecontVision");
        });

    state = updatePtzSettings(std::move(state), cameras);

    state.devicesDescription.hasCustomMediaPortCapability = combinedValue(cameras,
        [](const Camera& camera)
        {
            return camera->hasCameraCapabilities(
                nx::vms::api::DeviceCapability::customMediaPorts);
        });

    state.devicesDescription.hasCustomMediaPort = combinedValue(cameras,
        [](const Camera& camera)
        {
            return camera->mediaPort() > 0;
        });

    state.motion.dependingOnDualStreaming = combinedValue(cameras,
        &isMotionDetectionDependingOnDualStreaming);

    fetchFromCameras<bool>(state.recording.enabled, cameras,
        [](const auto& camera) { return camera->isScheduleEnabled(); });

    fetchFromCameras<bool>(state.audioEnabled, cameras,
        [](const Camera& camera) { return camera->isAudioEnabled(); });
    fetchFromCameras<bool>(state.twoWayAudioEnabled, cameras,
        [](const Camera& camera) { return camera->isTwoWayAudioEnabled(); });

    fetchFromCameras<QString>(state.credentials.login, cameras,
        [](const Camera& camera) { return camera->getAuth().user(); });

    // Password value aren't accessible by the client and doesn't fetched. state.credentials.login
    // that hasn't value is interpreted as "Unknown remote stored password" for the single camera
    // or as "Multiple values" for the multiple camera selection.

    fetchFromCameras<bool>(state.expert.dualStreamingDisabled, cameras,
        [](const Camera& camera) { return camera->isDualStreamingDisabled(); });

    fetchFromCameras<bool>(state.expert.cameraControlDisabled,
        cameras.filtered([](const Camera& camera) { return camera->isVideoQualityAdjustable(); }),
        [](const Camera& camera) { return camera->isCameraControlDisabledInternal(); });

    fetchFromCameras<bool>(state.expert.keepCameraTimeSettings,
        cameras,
        [](const Camera& camera) { return camera->keepCameraTimeSettings(); });

    fetchFromCameras<bool>(state.expert.useBitratePerGOP, cameras,
        [](const Camera& camera) { return camera->useBitratePerGop(); });

    state.expert.areOnvifSettingsApplicable = std::any_of(cameras.cbegin(), cameras.cend(),
        [](const Camera& camera) { return camera->isOnvifDevice(); });

    fetchFromCameras<UsingOnvifMedia2Type>(state.expert.useMedia2ToFetchProfiles, cameras,
        [](const Camera& camera) { return camera->useMedia2ToFetchProfiles(); });

    fetchFromCameras<bool>(state.expert.primaryRecordingDisabled, cameras,
        [](const Camera& camera)
        {
            return !camera->isPrimaryStreamRecorded();
        });

    fetchFromCameras<bool>(state.expert.recordAudioEnabled, cameras,
        [](const Camera& camera)
        {
            return camera->isAudioRecorded();
        });

    fetchFromCameras<bool>(state.expert.secondaryRecordingDisabled, cameras,
        [](const Camera& camera)
        {
            return !camera->isSecondaryStreamRecorded();
        });

    fetchFromCameras<vms::api::RtpTransportType>(state.expert.rtpTransportType, cameras,
        [](const Camera& camera)
        {
            return nx::reflect::fromString<vms::api::RtpTransportType>(
                camera->getProperty(QnMediaResource::rtpTransportKey()).toStdString(),
                vms::api::RtpTransportType::automatic);
        });
    fetchFromCameras<QString>(state.expert.forcedPrimaryProfile, cameras,
        [](const Camera& camera) { return camera->forcedProfile(nx::vms::api::StreamIndex::primary); });
    fetchFromCameras<QString>(state.expert.forcedSecondaryProfile, cameras,
        [](const Camera& camera) { return camera->forcedProfile(nx::vms::api::StreamIndex::secondary); });

    fetchFromCameras<nx::vms::common::RemoteArchiveSyncronizationMode>(
        state.expert.remoteArchiveSyncronizationMode, cameras,
        [](const Camera& camera)
        {
            return camera->getRemoteArchiveSynchronizationMode();
        });

    if (state.expert.areOnvifSettingsApplicable)
    {
        bool firstStep = true;
        for (const auto& camera: cameras)
        {
            if (firstStep)
            {
                state.expert.availableProfiles = camera->availableProfiles();
            }
            else
            {
                state.expert.availableProfiles = intersectProfiles(
                    state.expert.availableProfiles, (camera->availableProfiles()));
            }
            firstStep = false;
        }
    }

    fetchFromCameras<bool>(state.expert.trustCameraTime, cameras,
        [](const Camera& camera)
        {
            return camera->trustCameraTime();
        });

    fetchFromCameras<bool>(state.expert.remoteMotionDetectionEnabled, cameras,
        [](const Camera& camera) { return camera->isRemoteArchiveMotionDetectionEnabled(); });

    fetchFromCameras<int>(state.expert.customWebPagePort, cameras,
        [](const Camera& camera) { return camera->customWebPagePort(); });

    fetchFromCameras<int>(state.expert.customMediaPort, cameras,
        [](const Camera& camera) { return camera->mediaPort(); });
    if (state.expert.customMediaPort.hasValue() && state.expert.customMediaPort() > 0)
        state.expert.customMediaPortDisplayValue = state.expert.customMediaPort();

    if (state.devicesDescription.isVirtualCamera == CombinedValue::All)
    {
        fetchFromCameras<bool>(state.virtualCameraMotion.enabled, cameras,
            [](const Camera& camera)
            {
                return camera->isRemoteArchiveMotionDetectionEnabled();
            });

        fetchFromCameras<int>(state.virtualCameraMotion.sensitivity, cameras,
            [](const Camera& camera)
            {
                NX_ASSERT(camera->getVideoLayout()->channelCount() == 1);
                QnMotionRegion region = camera->getMotionRegion();
                const auto rects = region.getAllMotionRects();
                return rects.empty()
                    ? QnMotionRegion::kDefaultSensitivity
                    : rects.begin().key();
            });
    }

    if (singleCamera)
    {
        NX_VERBOSE(NX_SCOPE_TAG, "Single camera %1 is selected", singleCamera);
        state = loadSingleCameraProperties(
            std::move(state),
            singleCamera,
            deviceAgentSettingsAdapter,
            analyticsEnginesWatcher,
            advancedParametersManifestManager);
    }
    else
    {
        fetchFromCameras<bool>(state.motion.enabled, cameras, &isMotionDetectionEnabled);
    }

    state.motion.streamAlert = calculateMotionStreamAlert(state);

    state.recording.parametersAvailable = calculateRecordingParametersAvailable(cameras);

    Qn::calculateMaxFps(
        cameras,
        &state.devicesDescription.maxFps);

    state.imageControl = calculateImageControlSettings(cameras);

    state.isDefaultExpertSettings = isDefaultExpertSettings(state);

    state.recording.schedule = {};

    fetchFromCameras<ScheduleTasks>(state.recording.schedule, cameras,
        [](const auto& camera)
        {
            return calculateRecordingSchedule(camera);
        });
    state.scheduleAlerts = calculateScheduleAlerts(state);

    if (const auto& schedule = state.recording.schedule;
         schedule.hasValue() && !schedule().empty())
    {
        // Setup brush as the highest quality task present.
        const auto tasks = schedule();
        const auto highestQualityTask = std::max_element(tasks.cbegin(), tasks.cend(),
            [](const auto& l, const auto& r) { return l.fps < r.fps; });

        state.recording.brush.fps = highestQualityTask->fps;
        state.recording.brush.streamQuality = highestQualityTask->streamQuality;
        state.recording.brush.bitrateMbitPerSec =
            common::CameraBitrateCalculator::roundKbpsToMbps(highestQualityTask->bitrateKbps);
    }

    if (state.recording.brush.streamQuality == nx::vms::api::StreamQuality::undefined)
    {
        const auto schedule = nx::vms::common::defaultSchedule(state.recording.brush.fps);
        state.recording.brush.streamQuality = schedule[0].streamQuality;
    }

    state.recording.brush.recordingType = RecordingType::always;
    state.recording.brush.metadataTypes = {};
    state.recording.metadataRadioButton = State::MetadataRadioButton::motion;
    // In case only objects are supported.
    state.recording.metadataRadioButton = fixupMetadataRadioButton(state);

    // Default brush fps value.
    if (state.recording.brush.fps == 0)
    {
        state.recording.brush.fps = state.maxRecordingBrushFps();
    }
    else
    {
        state.recording.brush.fps =
            std::min(state.recording.brush.fps, state.maxRecordingBrushFps());
    }

    fetchFromCameras<int>(state.recording.thresholds.beforeSec, cameras,
        calculateRecordingThresholdBefore);
    fetchFromCameras<int>(state.recording.thresholds.afterSec, cameras,
        calculateRecordingThresholdAfter);

    state = loadMinMaxCustomBitrate(std::move(state));
    state = fillBitrateFromFixedQuality(std::move(state));

    state.recording.minPeriod = RecordingPeriod::minPeriod(cameras);
    state.recording.maxPeriod = RecordingPeriod::maxPeriod(cameras);

    if (!state.isPageVisible(state.selectedTab))
        state.selectedTab = CameraSettingsTab::general;

    return state;
}

State CameraSettingsDialogStateReducer::handleMotionTypeChanged(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    state.motion.dependingOnDualStreaming = combinedValue(cameras,
        &isMotionDetectionDependingOnDualStreaming);
    fetchFromCameras<bool>(state.motion.enabled, cameras, &isMotionDetectionEnabled);
    state = handleStreamParametersChange(std::move(state));

    Qn::calculateMaxFps(
        cameras,
        &state.devicesDescription.maxFps);

    return state;
}

State CameraSettingsDialogStateReducer::handleMotionForcedChanged(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    state.motion.dependingOnDualStreaming = combinedValue(cameras,
        &isMotionDetectionDependingOnDualStreaming);

    const bool isMotionForced = ((state.isSingleCamera() && NX_ASSERT(cameras.size() == 1)))
        ? cameras.front()->motionStreamIndex().isForced
        : false;
    state.motion.forced.setBase(isMotionForced);
    state = handleStreamParametersChange(std::move(state));

    return state;
}

State CameraSettingsDialogStateReducer::handleDualStreamingChanged(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    fetchFromCameras<bool>(state.expert.dualStreamingDisabled, cameras,
        [](const Camera& camera) { return camera->isDualStreamingDisabled(); });

    state.motion.dependingOnDualStreaming = combinedValue(cameras,
        &isMotionDetectionDependingOnDualStreaming);

    const StreamIndex streamIndex = (state.isSingleCamera() && NX_ASSERT(cameras.size() == 1))
        ? cameras.front()->motionStreamIndex().index
        : StreamIndex::undefined;
    state.motion.stream.setBase(streamIndex);

    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state = handleStreamParametersChange(std::move(state));

    return state;
}

State CameraSettingsDialogStateReducer::handleMediaStreamsChanged(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    state.motion.dependingOnDualStreaming = combinedValue(cameras,
        &isMotionDetectionDependingOnDualStreaming);

    if (state.isSingleCamera() && NX_ASSERT(cameras.size() == 1))
    {
        const auto camera = cameras.front();
        state.singleCameraProperties.primaryStreamResolution = camera->streamInfo(
            StreamIndex::primary).getResolution();
        state.singleCameraProperties.secondaryStreamResolution = camera->streamInfo(
            StreamIndex::secondary).getResolution();
    }

    state = handleStreamParametersChange(std::move(state));

    return state;
}

State CameraSettingsDialogStateReducer::handleMotionStreamChanged(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    state.motion.dependingOnDualStreaming = combinedValue(cameras,
        &isMotionDetectionDependingOnDualStreaming);

    const StreamIndex streamIndex = (state.isSingleCamera() && NX_ASSERT(cameras.size() == 1))
        ? cameras.front()->motionStreamIndex().index
        : StreamIndex::undefined;
    state.motion.stream.setBase(streamIndex);

    state = handleStreamParametersChange(std::move(state));

    return state;
}

State CameraSettingsDialogStateReducer::handleMotionRegionsChanged(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    if (state.isSingleCamera() && NX_ASSERT(cameras.size() == 1))
        state.motion.regionList.setBase(calculateMotionRegionList(state, cameras.front()));
    else
        state.motion.regionList.setBase({});
    return state;
}

State CameraSettingsDialogStateReducer::handleStreamUrlsChanged(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    if (state.isSingleCamera() && NX_ASSERT(cameras.size() == 1))
        state = loadStreamUrls(std::move(state), cameras.front());

    return state;
}

State CameraSettingsDialogStateReducer::handleCompatibleObjectTypesMaybeChanged(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    if (state.isSingleCamera() && NX_ASSERT(cameras.size() == 1))
    {
        state.singleCameraProperties.supportedAnalyicsObjectTypes =
            cameras.front()->supportedObjectTypes(/*filterByEngines*/ false);
        state.devicesDescription.hasObjectDetection =
            calculateSingleCameraObjectDetectionSupport(state);
    }
    else
    {
        state.devicesDescription.hasObjectDetection = calculateObjectDetectionSupport(
            state,
            cameras);
    }
    state = fixupRecordingBrush(std::move(state));
    state.scheduleAlerts = calculateScheduleAlerts(state);

    return state;
}

State CameraSettingsDialogStateReducer::handleStatusChanged(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    state.devicesDescription.hasMotion = combinedValue(cameras, &hasMotion);
    if (!state.isPageVisible(state.selectedTab)) //< Motion tab can become hidden.
        state.selectedTab = CameraSettingsTab::general;

    fetchFromCameras<ScheduleTasks>(state.recording.schedule, cameras,
        [](const auto& camera)
        {
            return calculateRecordingSchedule(camera);
        });
    state = handleStreamParametersChange(std::move(state));

    Qn::calculateMaxFps(
        cameras,
        &state.devicesDescription.maxFps);

    return state;
}

State CameraSettingsDialogStateReducer::handleMediaCapabilitiesChanged(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for cameras %2", __func__, cameras);

    state.devicesDescription.hasDualStreamingCapability = combinedValue(cameras,
        [](const Camera& camera) { return camera->hasDualStreamingInternal(); });
    fetchFromCameras<bool>(state.expert.secondaryRecordingDisabled, cameras,
        [](const Camera& camera) { return !camera->isSecondaryStreamRecorded(); });
    state = handleDualStreamingChanged(std::move(state), cameras);
    state = handleMotionStreamChanged(std::move(state), cameras);
    return state;
}

State CameraSettingsDialogStateReducer::setSingleCameraUserName(State state, const QString& text)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, text);

    const auto trimmedName = text.trimmed().remove('\n');
    if (trimmedName.isEmpty())
        return state;

    state.hasChanges = true;
    state.singleCameraProperties.name.setUser(trimmedName);
    return state;
}

std::pair<bool, State> CameraSettingsDialogStateReducer::setSingleCameraIsOnline(
    State state,
    bool isOnline)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, isOnline);

    NX_ASSERT(state.isSingleCamera());
    if (state.singleCameraProperties.isOnline == isOnline)
        return {false, std::move(state)};

    state.singleCameraProperties.isOnline = isOnline;
    return {true, std::move(state)};
}

State CameraSettingsDialogStateReducer::setScheduleBrush(
    State state,
    const RecordScheduleCellData& brush)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, nx::reflect::json::serialize(brush));

    state.recording.brush = brush;
    const auto fps = qBound(
        kMinFps,
        state.recording.brush.fps,
        state.maxRecordingBrushFps());

    state = state.recording.brush.isAutoBitrate()
        ? fillBitrateFromFixedQuality(std::move(state))
        : setRecordingBitrateMbps(std::move(state), brush.bitrateMbitPerSec);

    state = setScheduleBrushFps(std::move(state), fps);
    state.recordingHint = State::RecordingHint::brushChanged;

    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushRecordingType(
    State state,
    RecordingType value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    const bool isAllowed =
        [&]()
        {
            switch (value)
            {
                case RecordingType::metadataAndLowQuality:
                    return state.isDualStreamingEnabled();
                case RecordingType::metadataOnly:
                    return state.isMotionDetectionActive() || state.isObjectDetectionSupported();
                default:
                    return true;
            }
        }();

    if (!NX_ASSERT(isAllowed))
        return state;

    state.recording.brush.recordingType = value;

    switch (value)
    {
        case RecordingType::always:
        case RecordingType::never:
            state.recording.brush.metadataTypes = {};
            break;

        case RecordingType::metadataAndLowQuality:
            state.recording.brush.fps =
                std::clamp(state.recording.brush.fps, kMinFps, state.maxRecordingBrushFps());
            state.recording.brush.metadataTypes = State::radioButtonToMetadataTypes(
                state.recording.metadataRadioButton);
            break;
        case RecordingType::metadataOnly:
            state.recording.brush.metadataTypes = State::radioButtonToMetadataTypes(
                state.recording.metadataRadioButton);
            break;

        default:
            break;
    }

    state.recordingHint = State::RecordingHint::brushChanged;

    return state;
}

State CameraSettingsDialogStateReducer::setRecordingMetadataRadioButton(State state,
    State::MetadataRadioButton value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (!NX_ASSERT(isMetadataRadioButtonAllowed(state, value)))
        return state;

    state.recording.metadataRadioButton = value;
    if (state.recording.brush.recordingType == RecordingType::metadataAndLowQuality
        || state.recording.brush.recordingType == RecordingType::metadataOnly)
    {
        state.recording.brush.metadataTypes = State::radioButtonToMetadataTypes(value);
    }

    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushFps(State state, int value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    NX_ASSERT(qBound(kMinFps, value, state.maxRecordingBrushFps()) == value);
    state.recording.brush.fps = value;
    if (state.recording.brush.isAutoBitrate() || !state.recording.customBitrateAvailable)
    {
        // Lock quality.
        state = loadMinMaxCustomBitrate(std::move(state));
        state = fillBitrateFromFixedQuality(std::move(state));
    }
    else
    {
        // Lock normalized bitrate.
        const auto normalizedBitrate = state.recording.normalizedCustomBitrateMbps();
        state = loadMinMaxCustomBitrate(std::move(state));
        state = setRecordingBitrateNormalized(std::move(state), normalizedBitrate);
    }
    state.recordingHint = State::RecordingHint::brushChanged;

    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushQuality(
    State state,
    Qn::StreamQuality value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.recording.brush.streamQuality = value;
    state = fillBitrateFromFixedQuality(std::move(state));
    state.recordingHint = State::RecordingHint::brushChanged;

    return state;
}

State CameraSettingsDialogStateReducer::setSchedule(State state, const ScheduleTasks& schedule)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, nx::reflect::json::serialize(schedule));

    state.hasChanges = true;

    ScheduleTasks processed = schedule;
    if (!state.recording.customBitrateAvailable)
    {
        for (auto& task: processed)
            task.bitrateKbps = 0;
    }

    state.recording.schedule.setUser(processed);
    if (!state.recording.schedule.getBase().has_value())
        state.recording.schedule.setBase({});
    state.recordingHint = {};
    state.scheduleAlerts = calculateScheduleAlerts(state);
    return state;
}

State CameraSettingsDialogStateReducer::fixupSchedule(State state)
{
    NX_VERBOSE(NX_SCOPE_TAG, __func__);

    if (!NX_ASSERT(state.supportsSchedule()))
        return state;

    auto schedule = state.recording.schedule();

    for (auto& task: schedule)
    {
        if (!isValidScheduleTask(state, task))
        {
            task.recordingType = RecordingType::always;
            task.metadataTypes = {};
        }
    }

    state.hasChanges = true;
    state.recording.schedule.setUser(schedule);
    state.recordingHint = {};
    state.scheduleAlerts = calculateScheduleAlerts(state);
    NX_ASSERT(state.scheduleAlerts == State::ScheduleAlerts());

    return state;
}

State CameraSettingsDialogStateReducer::setRecordingShowFps(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);
    state.recording.showFps = value;
    return state;
}

State CameraSettingsDialogStateReducer::setRecordingShowQuality(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);
    state.recording.showQuality = value;
    return state;
}

State CameraSettingsDialogStateReducer::toggleCustomBitrateVisible(State state)
{
    NX_VERBOSE(NX_SCOPE_TAG, __func__);
    NX_ASSERT(state.recording.customBitrateAvailable);
    state.recording.customBitrateVisible = !state.recording.customBitrateVisible;
    return state;
}

State CameraSettingsDialogStateReducer::setRecordingBitrateMbps(State state, float mbps)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, mbps);

    NX_ASSERT(state.recording.customBitrateAvailable && state.recording.customBitrateVisible);
    state.recording.brush.bitrateMbitPerSec = mbps;
    state.recording.brush.streamQuality = calculateQualityForBitrateMbps(state, mbps);
    if (qFuzzyEquals(calculateBitrateForQualityMbps(state, state.recording.brush.streamQuality), mbps))
        state.recording.brush.bitrateMbitPerSec = RecordScheduleCellData::kAutoBitrateValue; //< Standard quality detected.
    state.recording.bitrateMbps = mbps;
    return state;
}

State CameraSettingsDialogStateReducer::setRecordingBitrateNormalized(
    State state, float value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    NX_ASSERT(state.recording.customBitrateAvailable && state.recording.customBitrateVisible);
    const auto spread = state.recording.maxBitrateMpbs - state.recording.minBitrateMbps;
    const auto mbps = state.recording.minBitrateMbps + value * spread;
    return setRecordingBitrateMbps(std::move(state), mbps);
}

State CameraSettingsDialogStateReducer::setMinRecordingPeriodAutomatic(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    auto& minPeriod = state.recording.minPeriod;
    state.hasChanges = true;

    if (value)
    {
        minPeriod.setAutoMode();
    }
    else
    {
        const auto& maxPeriod = state.recording.maxPeriod;
        if (maxPeriod.hasManualPeriodValue())
        {
            minPeriod.setManualModeWithPeriod(
                std::min(minPeriod.displayValue(), maxPeriod.displayValue()));
        }
        else
        {
            minPeriod.setManualMode();
        }
    }
    updateRecordingAlerts(state);
    return state;
}

State CameraSettingsDialogStateReducer::setMinRecordingPeriodValue(
    State state,
    std::chrono::seconds value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    auto& minPeriod = state.recording.minPeriod;
    NX_ASSERT(minPeriod.isManualMode());

    state.hasChanges = true;
    minPeriod.setManualModeWithPeriod(value);
    updateRecordingAlerts(state);

    auto& maxPeriod = state.recording.maxPeriod;
    if (maxPeriod.hasManualPeriodValue() && maxPeriod.displayValue() < value)
        maxPeriod.setManualModeWithPeriod(value);

    return state;
}

State CameraSettingsDialogStateReducer::setMaxRecordingPeriodAutomatic(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    auto& maxPeriod = state.recording.maxPeriod;
    state.hasChanges = true;

    if (value)
    {
        maxPeriod.setAutoMode();
    }
    else
    {
        const auto& minPeriod = state.recording.minPeriod;
        if (minPeriod.hasManualPeriodValue())
        {
            maxPeriod.setManualModeWithPeriod(
                std::max(minPeriod.displayValue(), maxPeriod.displayValue()));
        }
        else
        {
            maxPeriod.setManualMode();
        }
    }
    return state;
}

State CameraSettingsDialogStateReducer::setMaxRecordingPeriodValue(
    State state,
    std::chrono::seconds value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    auto& maxPeriod = state.recording.maxPeriod;
    NX_ASSERT(maxPeriod.isManualMode());

    state.hasChanges = true;
    maxPeriod.setManualModeWithPeriod(value);

    auto& minPeriod = state.recording.minPeriod;
    if (minPeriod.hasManualPeriodValue() && minPeriod.displayValue() > value)
        minPeriod.setManualModeWithPeriod(value);

    updateRecordingAlerts(state);

    return state;
}

State CameraSettingsDialogStateReducer::setRecordingBeforeThresholdSec(State state, int value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    NX_ASSERT(state.supportsRecordingByEvents());
    state.hasChanges = true;
    state.recording.thresholds.beforeSec.setUser(value);
    updateRecordingAlerts(state);
    return state;
}

State CameraSettingsDialogStateReducer::setRecordingAfterThresholdSec(State state, int value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    NX_ASSERT(state.supportsRecordingByEvents());
    state.hasChanges = true;
    state.recording.thresholds.afterSec.setUser(value);
    return state;
}

State CameraSettingsDialogStateReducer::setCustomAspectRatio(
    State state,
    const QnAspectRatio& value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    NX_ASSERT(state.imageControl.aspectRatioAvailable);
    state.hasChanges = true;
    state.imageControl.aspectRatio.setUser(value);
    return state;
}

State CameraSettingsDialogStateReducer::setCustomRotation(State state, StandardRotation value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    NX_ASSERT(state.imageControl.rotationAvailable);
    state.hasChanges = true;
    state.imageControl.rotation.setUser(value);
    return state;
}

std::pair<bool, State> CameraSettingsDialogStateReducer::setRecordingEnabled(
    State state,
    bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (state.recording.enabled.equals(value))
        return {false, std::move(state)};

    state.hasChanges = true;
    state.recording.enabled.setUser(value);
    if (!state.recording.enabled.getBase().has_value())
        state.recording.enabled.setBase(false);

    const bool emptyScheduleHintDisplayed = state.recordingHint.has_value()
        && *state.recordingHint == State::RecordingHint::emptySchedule;

    if (value)
    {
        const auto schedule = state.recording.schedule.valueOr({});
        const bool scheduleIsEmpty = schedule.empty()
            || std::all_of(schedule.cbegin(), schedule.cend(),
                [](const QnScheduleTask& task)
                {
                    return task.recordingType == RecordingType::never;
                });

        if (scheduleIsEmpty)
            state.recordingHint = State::RecordingHint::emptySchedule;
        else if (emptyScheduleHintDisplayed)
            state.recordingHint = {};
    }
    else if (emptyScheduleHintDisplayed)
    {
        state.recordingHint = {};
    }

    state.scheduleAlerts = calculateScheduleAlerts(state);

    return {true, std::move(state)};
}

State CameraSettingsDialogStateReducer::setAudioEnabled(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.audioEnabled.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setAudioInputDeviceId(State state, const QnUuid& deviceId)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, deviceId);

    if (!state.isSingleCamera())
        return state;

    state.singleCameraSettings.audioInputDeviceId = deviceId;
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setTwoWayAudioEnabled(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.twoWayAudioEnabled.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setAudioOutputDeviceId(State state, const QnUuid& deviceId)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, deviceId);

    if (!state.isSingleCamera())
        return state;

    state.singleCameraSettings.audioOutputDeviceId = deviceId;
    state.hasChanges = true;
    return state;
}

CameraSettingsDialogStateReducer::State CameraSettingsDialogStateReducer::setCameraHotspotsEnabled(
    State state,
    bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (!NX_ASSERT(state.isSingleCamera()))
        return state;

    state.singleCameraSettings.cameraHotspotsEnabled.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setCameraHotspotsData(
    State state,
    const nx::vms::common::CameraHotspotDataList& cameraHotspots)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, nx::reflect::json::serialize(cameraHotspots));

    if (!state.isSingleCamera())
        return state;

    if (state.singleCameraSettings.cameraHotspots.get() == cameraHotspots)
        return state;

    state.singleCameraSettings.cameraHotspots.setUser(cameraHotspots);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setMotionDetectionEnabled(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (!NX_ASSERT(state.isSingleCamera()))
        return state;

    if (value && state.isMotionImplicitlyDisabled())
        state.motion.forced.setUser(value);

    state.hasChanges = true;
    state.motion.enabled.setUser(value);

    if (!value)
        state.motionHint = {};
    else if (isCompletelyMaskedOut(state.motion.regionList()))
        state.motionHint = State::MotionHint::completelyMaskedOut;

    state = handleStreamParametersChange(std::move(state));

    return state;
}

State CameraSettingsDialogStateReducer::forceMotionDetection(State state)
{
    NX_VERBOSE(NX_SCOPE_TAG, __func__);

    state.hasChanges = true;
    state.motion.forced.setUser(true);
    state = handleStreamParametersChange(std::move(state));
    return state;
}

State CameraSettingsDialogStateReducer::setCurrentMotionSensitivity(State state, int value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (state.motion.currentSensitivity == value)
        return state;

    state.motion.currentSensitivity = value;

    int rectsCount = 0;
    for (const auto& region: state.motion.regionList())
        rectsCount += region.getMotionRectCount();

    if (rectsCount <= 1)
        state.motionHint = State::MotionHint::sensitivityChanged;
    return state;
}

State CameraSettingsDialogStateReducer::setMotionRegionList(
    State state,
    const QList<QnMotionRegion>& value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    const auto errorCode = validateMotionRegionList(state, value);
    switch (errorCode)
    {
        case QnMotionRegion::ErrorCode::Ok:
            state.hasChanges = true;
            state.motion.regionList.setUser(value);
            state.motionAlert = {};
            state.motionHint = {};
            return state;
        case QnMotionRegion::ErrorCode::Windows:
            state.motionAlert = State::MotionAlert::motionDetectionTooManyRectangles;
            return state;
        case QnMotionRegion::ErrorCode::Masks:
            state.motionAlert = State::MotionAlert::motionDetectionTooManyMaskRectangles;
            return state;
        case QnMotionRegion::ErrorCode::Sens:
            state.motionAlert = State::MotionAlert::motionDetectionTooManySensitivityRectangles;
            return state;
    }

    NX_ASSERT(false, "Invalid QnMotionRegion::ErrorCode: %1", (int) errorCode);
    return state;
}

std::pair<bool, State> CameraSettingsDialogStateReducer::setMotionDependingOnDualStreaming(
    State state, CombinedValue value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (state.motion.dependingOnDualStreaming == value)
        return {false, std::move(state)};

    state.motion.dependingOnDualStreaming = value;
    return {true, std::move(state)};
}

State CameraSettingsDialogStateReducer::setDewarpingParams(
    State state, const nx::vms::api::dewarping::MediaData& value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to value %2", __func__, nx::reflect::json::serialize(value));

    state.singleCameraSettings.dewarpingParams.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setIoPortDataList(
    State state, const QnIOPortDataList& value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to value %2", __func__, nx::reflect::json::serialize(value));

    if (!state.isSingleCamera() || state.devicesDescription.isIoModule != CombinedValue::All)
        return state;

    state.singleIoModuleSettings.ioPortsData.setUser(sortedPorts(value));
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setIoModuleVisualStyle(
    State state, vms::api::IoModuleVisualStyle value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (!state.isSingleCamera() || state.devicesDescription.isIoModule != CombinedValue::All)
        return state;

    state.singleIoModuleSettings.visualStyle.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setCameraControlDisabled(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    const bool hasArecontCameras =
        state.devicesDescription.isArecontCamera != CombinedValue::None;

    if (hasArecontCameras || !state.settingsOptimizationEnabled)
        return state;

    state.expert.cameraControlDisabled.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;

    state.expertAlerts.setFlag(State::ExpertAlert::cameraControlYielded,
        value && state.recording.parametersAvailable);

    return state;
}

CameraSettingsDialogStateReducer::State
    CameraSettingsDialogStateReducer::setKeepCameraTimeSettings(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (!state.settingsOptimizationEnabled)
        return state;

    state.expert.keepCameraTimeSettings.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;

    return state;
}

State CameraSettingsDialogStateReducer::setDualStreamingDisabled(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.hasChanges = true;
    state.expert.dualStreamingDisabled.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state = handleStreamParametersChange(std::move(state));
    return state;
}

State CameraSettingsDialogStateReducer::setUseBitratePerGOP(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (!state.settingsOptimizationEnabled)
        return state;

    state.expert.useBitratePerGOP.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setUseMedia2ToFetchProfiles(State state,
    UsingOnvifMedia2Type value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.expert.useMedia2ToFetchProfiles.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setPrimaryRecordingDisabled(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.hasChanges = true;
    state.expert.primaryRecordingDisabled.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state = handleStreamParametersChange(std::move(state));

    return state;
}

State CameraSettingsDialogStateReducer::setRecordAudioEnabled(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.hasChanges = true;
    state.expert.recordAudioEnabled.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state = handleStreamParametersChange(std::move(state));

    return state;
}

State CameraSettingsDialogStateReducer::setSecondaryRecordingDisabled(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (state.devicesDescription.hasDualStreamingCapability == CombinedValue::None)
        return state;

    state.hasChanges = true;
    state.expert.secondaryRecordingDisabled.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state = handleStreamParametersChange(std::move(state));

    return state;
}

State CameraSettingsDialogStateReducer::setPreferredPtzPresetType(
    State state, nx::core::ptz::PresetType value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (!state.canSwitchPtzPresetTypes())
        return state;

    state.expert.preferredPtzPresetType.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setForcedPtzPanTiltCapability(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (state.devicesDescription.canForcePanTiltCapabilities != CombinedValue::All)
        return state;

    state.expert.forcedPtzPanTiltCapability.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setForcedPtzZoomCapability(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (state.devicesDescription.canForceZoomCapability != CombinedValue::All)
        return state;

    state.expert.forcedPtzZoomCapability.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setDoNotSendStopPtzCommand(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (state.devicesDescription.hasPanTiltCapabilities != CombinedValue::All)
        return state;

    state.expert.doNotSendStopPtzCommand.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setRtpTransportType(
    State state, vms::api::RtpTransportType value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.expert.rtpTransportType.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setForcedPrimaryProfile(
    State state, const QString& value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.expert.forcedPrimaryProfile.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setForcedSecondaryProfile(
    State state, const QString& value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.expert.forcedSecondaryProfile.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setRemoteArchiveSyncronizationEnabled(
    State state,
    bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1", __func__);

    state.expert.remoteArchiveSyncronizationMode.setUser(
        value
        ? nx::vms::common::RemoteArchiveSyncronizationMode::automatic
        : nx::vms::common::RemoteArchiveSyncronizationMode::off);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setMotionStream(State state, StreamIndex value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (!NX_ASSERT(state.isSingleCamera()) || !state.motion.supportsSoftwareDetection)
        return state;

    state.hasChanges = true;
    state.motion.stream.setUser(value);
    state = handleStreamParametersChange(std::move(state));
    return state;
}

State CameraSettingsDialogStateReducer::setCustomMediaPortUsed(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (state.devicesDescription.hasCustomMediaPortCapability != CombinedValue::All)
        return state;

    const int customMediaPortValue = value ? state.expert.customMediaPortDisplayValue : 0;
    state.expert.customMediaPort.setUser(customMediaPortValue);
    state.devicesDescription.hasCustomMediaPort = value ? CombinedValue::All : CombinedValue::None;
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setCustomMediaPort(State state, int value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    NX_ASSERT(value > 0);
    if (state.devicesDescription.hasCustomMediaPortCapability != CombinedValue::All)
        return state;

    state.expert.customMediaPort.setUser(value);
    state.expert.customMediaPortDisplayValue = value;
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setCustomWebPagePort(State state, int value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    const bool enable = value > 0;
    state.expert.customWebPagePort.setUser(enable ? value : 0);
    state = updateCameraWebPage(std::move(state));
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setTrustCameraTime(State state, bool value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    state.expert.trustCameraTime.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setLogicalId(State state, int value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (!state.isSingleCamera())
        return state;

    state.singleCameraSettings.logicalId.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;

    return updateDuplicateLogicalIdInfo(std::move(state));
}

State CameraSettingsDialogStateReducer::generateLogicalId(State state)
{
    NX_VERBOSE(NX_SCOPE_TAG, __func__);

    if (!state.isSingleCamera())
        return state;

    auto cameras = qnClientCoreModule->resourcePool()->getAllCameras();
    std::set<int> usedValues;

    for (const auto& camera: cameras)
    {
        if (camera->getId() != state.singleCameraProperties.id)
            usedValues.insert(camera->logicalId());
    }

    int previousValue = 0;
    for (auto value: usedValues)
    {
        if (value > previousValue + 1)
            break;

        previousValue = value;
    }

    const auto newLogicalId = previousValue + 1;
    if (newLogicalId == state.singleCameraSettings.logicalId())
        return state;

    return setLogicalId(std::move(state), newLogicalId);
}

State CameraSettingsDialogStateReducer::resetExpertSettings(State state)
{
    NX_VERBOSE(NX_SCOPE_TAG, __func__);

    if (state.isDefaultExpertSettings)
        return state;

    state = setCameraControlDisabled(std::move(state), false);
    state = setKeepCameraTimeSettings(std::move(state), true);
    state = setDualStreamingDisabled(std::move(state), false);
    state = setUseBitratePerGOP(std::move(state), false);
    state = setUseMedia2ToFetchProfiles(std::move(state), UsingOnvifMedia2Type::autoSelect);
    state = setPrimaryRecordingDisabled(std::move(state), false);
    state = setSecondaryRecordingDisabled(std::move(state), false);
    state = setRecordAudioEnabled(std::move(state), true);
    state = setPreferredPtzPresetType(std::move(state), nx::core::ptz::PresetType::undefined);
    state = setForcedPtzPanTiltCapability(std::move(state), false);
    state = setForcedPtzZoomCapability(std::move(state), false);
    state = setDoNotSendStopPtzCommand(std::move(state), false);
    state = setRtpTransportType(std::move(state), nx::vms::api::RtpTransportType::automatic);
    state = setForcedPrimaryProfile(std::move(state), QString());
    state = setForcedSecondaryProfile(std::move(state), QString());
    state = setRemoteArchiveSyncronizationEnabled(std::move(state), false);
    state = setCustomWebPagePort(std::move(state), 0);
    state = setCustomMediaPortUsed(std::move(state), false);
    state = setTrustCameraTime(std::move(state), false);
    state = setLogicalId(std::move(state), {});

    state.isDefaultExpertSettings = true;
    return state;
}

std::pair<bool, State> CameraSettingsDialogStateReducer::setAnalyticsEngines(
    State state, const QList<AnalyticsEngineInfo>& value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, nx::reflect::json::serialize(value));

    if (state.analytics.engines == value)
        return {false, std::move(state)};

    state.analytics.engines = value;

    QnUuid currentEngineId = state.analytics.currentEngineId;
    if (value.empty() || !analyticsEngineIsPresentInList(state.analytics.currentEngineId, state))
        currentEngineId = {};

    if (currentEngineId != state.analytics.currentEngineId)
    {
        state.analytics.currentEngineId = currentEngineId;
        NX_VERBOSE(NX_SCOPE_TAG, "Current analytics engine reset to %1",
            state.analytics.currentEngineId);
    }

    if (!state.isPageVisible(state.selectedTab)) //< Analytics tab can become hidden.
        state.selectedTab = CameraSettingsTab::general;

    return {true, std::move(state)};
}

State CameraSettingsDialogStateReducer::handleOverusedEngines(
    State state, const QSet<QnUuid>& overusedEngines)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 for %2", __func__, overusedEngines);

    NX_ASSERT(state.isSingleCamera());

    if (!state.isSingleCamera())
        return state;

    const auto userEnabledEngineBase = state.analytics.userEnabledEngines.getBase();
    auto userEnabledEngineUser = state.analytics.userEnabledEngines.getBase();
    for (auto engineIt = userEnabledEngineUser.begin(); engineIt != userEnabledEngineUser.end();)
    {
        if (!userEnabledEngineBase.contains(*engineIt) && overusedEngines.contains(*engineIt))
            engineIt = userEnabledEngineUser.erase(engineIt);
        else
            ++engineIt;
    }

    state = setUserEnabledAnalyticsEngines(std::move(state), userEnabledEngineUser);

    return state;
}

std::pair<bool, State> CameraSettingsDialogStateReducer::setCurrentAnalyticsEngineId(
    State state, const QnUuid& value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    if (state.analytics.currentEngineId == value)
        return {false, std::move(state)};

    if (!value.isNull() && !analyticsEngineIsPresentInList(value, state))
        return {false, std::move(state)};

    state.analytics.currentEngineId = value;
    NX_VERBOSE(NX_SCOPE_TAG, "Current analytics engine selected as %1",
        state.analytics.currentEngineId);

    return {true, std::move(state)};
}

State CameraSettingsDialogStateReducer::setUserEnabledAnalyticsEngines(
    State state, const QSet<QnUuid>& value)
{
    NX_VERBOSE(NX_SCOPE_TAG, "%1 to %2", __func__, value);

    NX_ASSERT(state.isSingleCamera());

    // Filter out engines which are not available anymore.
    QSet<QnUuid> actualValue;
    for (const auto& id: value)
    {
        if (analyticsEngineIsPresentInList(id, state))
            actualValue.insert(id);
    }

    state.analytics.userEnabledEngines.setUser(actualValue);
    state.devicesDescription.hasObjectDetection =
        calculateSingleCameraObjectDetectionSupport(state);

    state = fixupRecordingBrush(std::move(state));
    state.scheduleAlerts = calculateScheduleAlerts(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setAnalyticsStreamIndex(
    State state, const QnUuid& engineId, StreamIndex value, ModificationSource source)
{
    if (source == ModificationSource::local)
    {
        if (!NX_ASSERT(state.analyticsStreamSelectionEnabled(engineId)))
            return state;

        state.analytics.streamByEngineId[engineId].setUser(value);
        state.hasChanges = true;
    }
    else if (NX_ASSERT(source == ModificationSource::remote))
    {
        auto& streamRef = state.analytics.streamByEngineId[engineId];
        streamRef.setBase(value);

        if (value == StreamIndex::undefined)
            streamRef.resetUser();
    }
    return state;
}

std::pair<bool, State> CameraSettingsDialogStateReducer::setDeviceAgentSettingsValues(
    State state,
    const QnUuid& engineId,
    const QString& activeElement,
    const QJsonObject& values,
    const QJsonObject& paramValues)
{
    if (!std::any_of(
        state.analytics.engines.begin(),
        state.analytics.engines.end(),
        [engineId](const auto& engine) { return engine.id == engineId; }))
    {
        return {false, std::move(state)};
    }

    auto& storedValues = state.analytics.settingsByEngineId[engineId].values;
    if (storedValues.get() == values && activeElement.isEmpty())
        return {false, std::move(state)};

    storedValues.setUser(values);
    state.hasChanges = true;

    if (!activeElement.isEmpty())
    {
        bool loading = qnClientModule->analyticsSettingsManager()->activeSettingsChanged(
            DeviceAgentId{state.singleCameraProperties.id, engineId},
            activeElement,
            state.analytics.settingsByEngineId[engineId].model,
            values,
            paramValues);

        state.analytics.settingsByEngineId[engineId].loading = loading;
    }

    return {true, std::move(state)};
}

State CameraSettingsDialogStateReducer::refreshDeviceAgentSettings(
    State state, const QnUuid& engineId)
{
    if (!NX_ASSERT(state.isSingleCamera()))
        return state;

    qnClientModule->analyticsSettingsManager()->refreshSettings(
        DeviceAgentId{state.singleCameraProperties.id, engineId});

    auto& settings = state.analytics.settingsByEngineId[engineId];
    settings.loading = true;

    NX_VERBOSE(NX_SCOPE_TAG, "Loading device agent settings for %1", engineId);
    return state;
}

std::pair<bool, State> CameraSettingsDialogStateReducer::resetDeviceAgentData(
    State state,
    const QnUuid& engineId,
    const DeviceAgentData& data,
    bool replaceUser)
{
    auto& settings = state.analytics.settingsByEngineId[engineId];
    settings.model = data.model;

    if (replaceUser)
    {
        settings.values.setUser(data.values);
    }
    else
    {
        settings.values.setBase(data.values);
        settings.values.resetUser();
    }

    settings.errors = data.errors;
    settings.loading = data.status != DeviceAgentData::Status::ok;

    NX_TRACE(NX_SCOPE_TAG, "Device agent settings reset for %1, status is %2, loading: %3",
        engineId, (int)data.status, settings.loading);

    return std::make_pair(true, std::move(state));
}

State CameraSettingsDialogStateReducer::setVirtualCameraIgnoreTimeZone(State state, bool value)
{
    state.virtualCameraIgnoreTimeZone = value;
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setVirtualCameraMotionDetectionEnabled(State state, bool value)
{
    if (state.devicesDescription.isVirtualCamera != CombinedValue::All)
        return state;

    state.virtualCameraMotion.enabled.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setVirtualCameraMotionSensitivity(State state, int value)
{
    if (state.devicesDescription.isVirtualCamera != CombinedValue::All)
        return state;

    state.virtualCameraMotion.sensitivity.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setCredentials(
    State state, const std::optional<QString>& login, const std::optional<QString>& password)
{
    if (!login && !password)
        return state;

    if (login)
        state.credentials.login.setUser(login->trimmed());
    if (password)
        state.credentials.password.setUser(password->trimmed());

    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setStreamUrls(
    State state,
    const QString& primary,
    const QString& secondary)
{
    if (!state.singleCameraProperties.editableStreamUrls || !state.isSingleCamera())
        return state;

    state.singleCameraSettings.primaryStream.setUser(primary);
    state.singleCameraSettings.secondaryStream.setUser(secondary);
    state.hasChanges = true;

    return state;
}

State CameraSettingsDialogStateReducer::setAdvancedSettingsManifest(
    State state,
    const QnCameraAdvancedParams& manifest)
{
    NX_ASSERT(state.isSingleCamera());
    state.singleCameraProperties.advancedSettingsManifest = manifest;
    return state;
}

State CameraSettingsDialogStateReducer::setDifferentPtzPanTiltSensitivities(State state, bool value)
{
    if (!NX_ASSERT(state.canAdjustPtzSensitivity()))
        return state;

    state.expert.ptzSensitivity.separate.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setPtzPanSensitivity(State state, qreal value)
{
    if (!NX_ASSERT(state.canAdjustPtzSensitivity()
        && state.expert.ptzSensitivity.separate.hasValue()))
    {
        return state;
    }

    state.expert.ptzSensitivity.pan.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setPtzTiltSensitivity(State state, qreal value)
{
    if (!NX_ASSERT(state.canAdjustPtzSensitivity()
        && state.expert.ptzSensitivity.separate.valueOr(false)))
    {
        return state;
    }
    state.expert.ptzSensitivity.tilt.setUser(value);
    state.hasChanges = true;
    return state;
}

CameraSettingsDialogStateReducer::State CameraSettingsDialogStateReducer::setHasExportPermission(
    State state, bool value)
{
    state.hasExportPermission = value;
    state.hasChanges = true;
    return state;
}

CameraSettingsDialogStateReducer::State CameraSettingsDialogStateReducer::setScreenRecordingOn(
    State state, bool value)
{
    state.screenRecordingOn = value;
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setRemoteArchiveMotionDetectionEnabled(State state,
    bool value)
{
    if (state.devicesDescription.hasRemoteArchiveCapability != CombinedValue::All)
        return state;

    state.expert.remoteMotionDetectionEnabled.setUser(value);
    state.hasChanges = true;
    return state;
}

bool CameraSettingsDialogStateReducer::isMotionDetectionDependingOnDualStreaming(
    const QnVirtualCameraResourcePtr& camera)
{
    if (!NX_ASSERT(camera)
        || !camera->hasDualStreaming()
        || camera->getMotionType() != MotionType::software)
    {
        return false;
    }

    const auto stream = camera->motionStreamIndex();
    if (stream.index == StreamIndex::primary || stream.isForced)
        return false;

    const auto size = camera->streamInfo(StreamIndex::primary).getResolution();
    const auto pixels = size.width() * size.height();
    return pixels > QnVirtualCameraResource::kMaximumMotionDetectionPixels;
}

} // namespace nx::vms::client::desktop
