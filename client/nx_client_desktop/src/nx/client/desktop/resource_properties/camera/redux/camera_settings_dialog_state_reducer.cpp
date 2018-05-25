#include "camera_settings_dialog_state_reducer.h"

#include <limits>

#include <camera/fps_calculator.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <core/resource_management/resource_pool.h>
#include <utils/camera/camera_bitrate_calculator.h>

#include <nx/fusion/model_functions.h>
#include <nx/vms/api/types/rtp_types.h>
#include <nx/vms/api/types/motion_types.h>
#include <nx/utils/algorithm/same.h>

namespace nx {
namespace client {
namespace desktop {

using State = CameraSettingsDialogState;
using RecordingThresholds = State::RecordingSettings::Thresholds;

using Camera = QnVirtualCameraResourcePtr;
using Cameras = QnVirtualCameraResourceList;

namespace {

static constexpr int kMinFps = 1;

static constexpr auto kMinQuality = Qn::StreamQuality::low;
static constexpr auto kMaxQuality = Qn::StreamQuality::highest;

template<class Data>
void fetchFromCameras(
    State::UserEditableMultiple<Data>& value,
    const Cameras& cameras,
    std::function<Data(const Camera&)> getter)
{
    Data data;
    if (utils::algorithm::same(cameras.cbegin(), cameras.cend(), getter, &data))
        value.setBase(data);
}

template<class Data, class Intermediate>
void fetchFromCameras(
    State::UserEditableMultiple<Data>& value,
    const Cameras& cameras,
    std::function<Intermediate(const Camera&)> getter,
    std::function<Data(const Intermediate&)> converter)
{
    Intermediate data;
    if (utils::algorithm::same(cameras.cbegin(), cameras.cend(), getter, &data))
        value.setBase(converter(data));
}

State::CombinedValue combinedValue(const Cameras& cameras,
    std::function<bool(const Camera&)> predicate)
{
    bool value;
    if (!utils::algorithm::same(cameras.cbegin(), cameras.cend(), predicate, &value))
        return State::CombinedValue::Some;

    return value ? State::CombinedValue::All : State::CombinedValue::None;
}

QString calculateWebPage(const Camera& camera)
{
    NX_ASSERT(camera);
    if (!camera)
        return QString();

    QString webPageAddress = lit("http://") + camera->getHostAddress();

    QUrl url = QUrl::fromUserInput(camera->getUrl());
    if (url.isValid())
    {
        const QUrlQuery query(url);
        int port = query.queryItemValue(lit("http_port")).toInt();
        if (port == 0)
            port = url.port(80);

        if (port != 80 && port > 0)
            webPageAddress += L':' + QString::number(url.port());
    }

    return lit("<a href=\"%1\">%1</a>").arg(webPageAddress);
}

State::FisheyeCalibrationSettings fisheyeCalibrationSettings(const QnMediaDewarpingParams& params)
{
    State::FisheyeCalibrationSettings calibration;
    calibration.offset = QPointF(params.xCenter - 0.5, params.yCenter - 0.5);
    calibration.radius = params.radius;
    calibration.aspectRatio = params.hStretch;
    return calibration;
}

bool isMotionDetectionEnabled(const Camera& camera)
{
    const auto motionType = camera->getMotionType();
    return motionType != Qn::MotionType::MT_NoMotion
        && camera->supportedMotionType().testFlag(motionType);
}

bool calculateRecordingParametersAvailable(const Cameras& cameras)
{
    return std::any_of(
        cameras.cbegin(),
        cameras.cend(),
        [](const Camera& camera)
        {
            return camera->hasVideo()
            // TODO: Probably server should set param NO_RECORDING_PARAMS_PARAM_NAME for rtsp links.
                && camera->getVendor() != lit("GENERIC_RTSP")
                && !camera->hasParam(Qn::NO_RECORDING_PARAMS_PARAM_NAME);
        });
}

float calculateBitrateForQualityMbps(const State& state, Qn::StreamQuality quality)
{
    return core::CameraBitrateCalculator::roundKbpsToMbps(
        core::CameraBitrateCalculator::suggestBitrateForQualityKbps(
            quality,
            state.recording.defaultStreamResolution,
            state.recording.brush.fps,
            state.recording.mediaStreamCapability,
            state.recording.bitratePerGopType));
}

Qn::StreamQuality calculateQualityForBitrateMbps(const State& state, float bitrateMbps)
{
    auto current = kMinQuality;
    auto currentBr = calculateBitrateForQualityMbps(state, current);

    for (int i = (int)current + 1; i <= (int)kMaxQuality; ++i)
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
    state.recording.minBitrateMbps = calculateBitrateForQualityMbps(state,
        Qn::StreamQuality::lowest);
    state.recording.maxBitrateMpbs = calculateBitrateForQualityMbps(state,
        Qn::StreamQuality::highest);
    return state;
}

State fillBitrateFromFixedQuality(State state)
{
    state.recording.brush.bitrateMbps = ScheduleCellParams::kAutomaticBitrate;
    state.recording.bitrateMbps = calculateBitrateForQualityMbps(
        state,
        state.recording.brush.quality);
    return state;
}

State loadNetworkInfo(State state, const Camera& camera)
{
    NX_ASSERT(camera);
    if (!camera)
        return state;

    state.singleCameraProperties.ipAddress = QnResourceDisplayInfo(camera).host();
    state.singleCameraProperties.webPage = calculateWebPage(camera);

    const bool isIoModule = camera->isIOModule();
    const bool hasPrimaryStream = !isIoModule || camera->isAudioSupported();
    if (hasPrimaryStream)
        state.singleCameraProperties.primaryStream = camera->sourceUrl(Qn::CR_LiveVideo);

    const bool hasSecondaryStream = camera->hasDualStreaming();
    if (hasSecondaryStream)
        state.singleCameraProperties.secondaryStream = camera->sourceUrl(Qn::CR_SecondaryLiveVideo);

    return state;
}

State::RecordingDays calculateMinRecordingDays(const Cameras& cameras)
{
    if (cameras.empty())
        return {vms::api::kDefaultMinArchiveDays, true, true};

    // Any negative min days value means 'auto'. Storing absolute value to keep previous one.
    auto calcMinDays = [](int d) { return d == 0 ? vms::api::kDefaultMinArchiveDays : qAbs(d); };

    const int minDays = (*std::min_element(
        cameras.cbegin(),
        cameras.cend(),
        [calcMinDays](
        const Camera& l,
        const Camera& r)
        {
            return calcMinDays(l->minDays()) < calcMinDays(r->minDays());
        }))->minDays();

    const bool isAuto = minDays <= 0;
    const bool sameMinDays = std::all_of(
        cameras.cbegin(),
        cameras.cend(),
        [minDays, isAuto](const Camera& camera)
        {
            return isAuto
                ? camera->minDays() <= 0
                : camera->minDays() == minDays;
        });
    return {calcMinDays(minDays), isAuto, sameMinDays};
}

State::RecordingDays calculateMaxRecordingDays(const Cameras& cameras)
{
    if (cameras.empty())
        return {vms::api::kDefaultMaxArchiveDays, true, true};

    /* Any negative max days value means 'auto'. Storing absolute value to keep previous one. */
    auto calcMaxDays = [](int d) { return d == 0 ? vms::api::kDefaultMaxArchiveDays : qAbs(d); };

    const int maxDays = (*std::max_element(
        cameras.cbegin(),
        cameras.cend(),
        [calcMaxDays](
        const Camera& l,
        const Camera& r)
        {
            return calcMaxDays(l->maxDays()) < calcMaxDays(r->maxDays());
        }))->maxDays();

    const bool isAuto = maxDays <= 0;
    const bool sameMaxDays = std::all_of(
        cameras.cbegin(),
        cameras.cend(),
        [maxDays, isAuto](const Camera& camera)
        {
            return isAuto
                ? camera->maxDays() <= 0
                : camera->maxDays() == maxDays;
        });
    return {calcMaxDays(maxDays), isAuto, sameMaxDays};
}

State::ImageControlSettings calculateImageControlSettings(
    const Cameras& cameras)
{
    const bool hasVideo = std::all_of(cameras.cbegin(), cameras.cend(),
        [](const auto& camera) { return camera->hasVideo(); });

    State::ImageControlSettings result;
    result.aspectRatioAvailable = hasVideo && std::all_of(cameras.cbegin(), cameras.cend(),
        [](const auto& camera) { return !camera->hasFlags(Qn::wearable_camera); });

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
        fetchFromCameras<Rotation>(
            result.rotation,
            cameras,
            [](const auto& camera)
            {
                const auto rotationString = camera->getProperty(QnMediaResource::rotationKey());
                return Rotation::closestStandardRotation(rotationString.toInt());
            });
    }

    return result;
}

QnScheduleTaskList calculateRecordingSchedule(const Camera& camera)
{
    if (camera->isDtsBased())
        return {};

    return camera->getScheduleTasks();
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

QnMotionRegion::ErrorCode validateMotionRegionList(const State& state,
    const QList<QnMotionRegion>& regionList)
{
    if (!state.singleCameraProperties.motionConstraints)
        return QnMotionRegion::ErrorCode::Ok;

    for (const auto& region: regionList)
    {
        const auto errorCode = region.isValid(
            state.singleCameraProperties.motionConstraints->maxTotalRects,
            state.singleCameraProperties.motionConstraints->maxMaskRects,
            state.singleCameraProperties.motionConstraints->maxSensitiveRects);

        if (errorCode != QnMotionRegion::ErrorCode::Ok)
            return errorCode;
    }

    return QnMotionRegion::ErrorCode::Ok;
}

vms::api::MotionStreamType motionStreamType(const Camera& camera)
{
    return QnLexical::deserialized<vms::api::MotionStreamType>(
        camera->getProperty(QnMediaResource::motionStreamKey()),
        vms::api::MotionStreamType::automatic);
}

State updateDuplicateLogicalIdInfo(State state)
{
    state.singleCameraSettings.sameLogicalIdCameraNames.clear();

    const QnUuid currentId(state.singleCameraProperties.id);
    const auto currentLogicalId = state.singleCameraSettings.logicalId();

    const auto isSameLogicalId =
        [&currentId, &currentLogicalId](const Camera& camera)
        {
            return !camera->getLogicalId().isEmpty()
                && camera->getLogicalId().toInt() == currentLogicalId
                && camera->getId() != currentId;
        };

    const auto duplicateCameras = qnClientCoreModule->commonModule()->resourcePool()->
        getAllCameras().filtered(isSameLogicalId);

    for (const auto& camera: duplicateCameras)
        state.singleCameraSettings.sameLogicalIdCameraNames << camera->getName();

    return state;
}

bool isDefaultExpertSettings(const State& state)
{
    if (state.isSingleCamera() && state.singleCameraSettings.logicalId() > 0)
        return false;

    if (state.settingsOptimizationEnabled)
    {
        if ((state.devicesDescription.isArecontCamera == State::CombinedValue::None
                && state.expert.cameraControlDisabled.valueOr(true))
            || (state.devicesDescription.hasPredefinedBitratePerGOP == State::CombinedValue::None
                && state.expert.useBitratePerGOP.valueOr(true))
            || state.expert.dualStreamingDisabled.valueOr(true))
        {
            return false;
        }
    }

    if (state.expert.primaryRecordingDisabled.valueOr(true)
        || (state.devicesDescription.hasDualStreamingCapability != State::CombinedValue::None
            && state.expert.secondaryRecordingDisabled.valueOr(true)))
    {
        return false;
    }

    if (state.devicesDescription.canDisableNativePtzPresets != State::CombinedValue::None
        && state.expert.nativePtzPresetsDisabled.valueOr(true))
    {
        return false;
    }

    if (state.devicesDescription.supportsMotionStreamOverride == State::CombinedValue::All
        && state.expert.motionStreamType() != vms::api::MotionStreamType::automatic)
    {
        return false;
    }

    return state.expert.rtpTransportType.hasValue()
        && state.expert.rtpTransportType() == vms::api::RtpTransportType::automatic;
}

} // namespace

State CameraSettingsDialogStateReducer::applyChanges(State state)
{
    NX_EXPECT(!state.readOnly);
    state.hasChanges = false;
    return state;
}

State CameraSettingsDialogStateReducer::setReadOnly(State state, bool value)
{
    state.readOnly = value;
    return state;
}

State CameraSettingsDialogStateReducer::setSettingsOptimizationEnabled(State state, bool value)
{
    state.settingsOptimizationEnabled = value;
    return state;
}

State CameraSettingsDialogStateReducer::setGlobalPermissions(
    State state, Qn::GlobalPermissions value)
{
    state.globalPermissions = value;
    return state;
}

State CameraSettingsDialogStateReducer::setSingleWearableState(
    State state, const WearableState& value)
{
    state.singleWearableState = value;
    state.wearableUploaderName = QString();

    if (state.singleWearableState.status == WearableState::LockedByOtherClient)
    {
        if (const auto user = qnClientCoreModule->commonModule()->resourcePool()->getResourceById(
            state.singleWearableState.lockUserId))
        {
            state.wearableUploaderName = user->getName();
        }
    }

    return state;
}

State CameraSettingsDialogStateReducer::loadCameras(
    State state,
    const Cameras& cameras)
{
    const auto firstCamera = cameras.empty()
        ? Camera()
        : cameras.first();

    state.hasChanges = false;
    state.singleCameraProperties = {};
    state.singleCameraSettings = {};
    state.singleIoModuleSettings = {};
    state.devicesDescription = {};
    state.expert = {};
    state.recording = {};
    state.wearableMotion = {};
    state.devicesCount = cameras.size();
    state.alert = {};

    state.deviceType = firstCamera
        ? QnDeviceDependentStrings::calculateDeviceType(firstCamera->resourcePool(), cameras)
        : QnCameraDeviceType::Mixed;

    state.devicesDescription.isDtsBased = combinedValue(cameras,
        [](const Camera& camera) { return camera->isDtsBased(); });
    state.devicesDescription.isWearable = combinedValue(cameras,
        [](const Camera& camera) { return camera->hasFlags(Qn::wearable_camera); });
    state.devicesDescription.isIoModule = combinedValue(cameras,
        [](const Camera& camera) { return camera->isIOModule(); });
    state.devicesDescription.hasMotion = combinedValue(cameras,
        [](const Camera& camera) { return camera->hasMotion(); });
    state.devicesDescription.hasDualStreamingCapability = combinedValue(cameras,
        [](const Camera& camera) { return camera->hasDualStreamingInternal(); });

    state.devicesDescription.hasRemoteArchiveCapability = combinedValue(cameras,
        [](const Camera& camera)
        {
            return camera->hasCameraCapabilities(Qn::RemoteArchiveCapability);
        });

    state.devicesDescription.isArecontCamera = combinedValue(cameras,
        [](const Camera& camera)
        {
            const auto cameraType = qnResTypePool->getResourceType(camera->getTypeId());
            return cameraType && cameraType->getManufacture() == lit("ArecontVision");
        });

    state.devicesDescription.hasPredefinedBitratePerGOP = combinedValue(cameras,
        [](const Camera& camera)
        {
            return camera->bitratePerGopType() == Qn::BPG_Predefined;
        });

    state.devicesDescription.hasPtzPresets = combinedValue(cameras,
        [](const Camera& camera)
        {
            return camera->hasAnyOfPtzCapabilities(Ptz::PresetsPtzCapability);
        });

    state.devicesDescription.canDisableNativePtzPresets = combinedValue(cameras,
        [](const Camera& camera) { return camera->canDisableNativePtzPresets(); });

    state.devicesDescription.supportsMotionStreamOverride = combinedValue(cameras,
        [](const Camera& camera)
        {
            return camera->supportedMotionType().testFlag(Qn::MotionType::MT_SoftwareGrid);
        });

    if (firstCamera)
    {
        auto& singleProperties = state.singleCameraProperties;
        singleProperties.name.setBase(firstCamera->getName());
        singleProperties.id = firstCamera->getId().toSimpleString();
        singleProperties.firmware = firstCamera->getFirmware();
        singleProperties.macAddress = firstCamera->getMAC().toString();
        singleProperties.model = firstCamera->getModel();
        singleProperties.vendor = firstCamera->getVendor();
        singleProperties.hasVideo = firstCamera->hasVideo();

        if (firstCamera->getDefaultMotionType() == Qn::MotionType::MT_HardwareGrid)
        {
            singleProperties.motionConstraints = State::MotionConstraints();
            singleProperties.motionConstraints->maxTotalRects = firstCamera->motionWindowCount();
            singleProperties.motionConstraints->maxMaskRects = firstCamera->motionMaskWindowCount();
            singleProperties.motionConstraints->maxSensitiveRects
                = firstCamera->motionSensWindowCount();
        }

        const auto fisheyeParams = firstCamera->getDewarpingParams();
        state.singleCameraSettings.enableFisheyeDewarping.setBase(fisheyeParams.enabled);
        state.singleCameraSettings.fisheyeMountingType.setBase(fisheyeParams.viewMode);
        state.singleCameraSettings.fisheyeFovRotation.setBase(fisheyeParams.fovRot);
        state.singleCameraSettings.fisheyeCalibrationSettings.setBase(
            fisheyeCalibrationSettings(fisheyeParams));

        state = loadNetworkInfo(std::move(state), firstCamera);

        state.recording.bitratePerGopType = firstCamera->bitratePerGopType();
        state.recording.defaultStreamResolution = firstCamera->streamInfo().getResolution();
        state.recording.mediaStreamCapability = firstCamera->cameraMediaCapability().
            streamCapabilities.value(Qn::StreamIndex::primary);

        state.recording.customBitrateAvailable = true;
        state = loadMinMaxCustomBitrate(std::move(state));

        state.singleCameraSettings.enableMotionDetection.setBase(
            isMotionDetectionEnabled(firstCamera));

        auto regionList = firstCamera->getMotionRegionList();
        const int channelCount = firstCamera->getVideoLayout()->channelCount();

        if (regionList.size() > channelCount)
            regionList.erase(regionList.begin() + channelCount, regionList.end());
        else while (regionList.size() < channelCount)
            regionList << QnMotionRegion();

        if (validateMotionRegionList(state, regionList) != QnMotionRegion::ErrorCode::Ok)
        {
            for (auto& region: regionList)
                region = QnMotionRegion(); //< Reset to default.
        }

        state.singleCameraSettings.motionRegionList.setBase(regionList);

        if (firstCamera->isIOModule())
        {
            state.singleIoModuleSettings.visualStyle.setBase(
                QnLexical::deserialized<vms::api::IoModuleVisualStyle>(
                    firstCamera->getProperty(Qn::IO_OVERLAY_STYLE_PARAM_NAME), {}));

            state.singleIoModuleSettings.ioPortsData.setBase(firstCamera->getIOPorts());
        }

        state.singleCameraSettings.logicalId.setBase(firstCamera->getLogicalId().toInt());
        state = updateDuplicateLogicalIdInfo(std::move(state));

        Qn::calculateMaxFps(
            {firstCamera},
            &state.singleCameraProperties.maxFpsWithoutMotion,
            nullptr,
            false);
    }

    state.recording.enabled = {};
    fetchFromCameras<bool>(state.recording.enabled, cameras,
        [](const auto& camera) { return camera->isLicenseUsed(); });

    state.recording.parametersAvailable = calculateRecordingParametersAvailable(cameras);

    Qn::calculateMaxFps(
            cameras,
            &state.devicesDescription.maxFps,
            &state.devicesDescription.maxDualStreamingFps,
            false);

    state.recording.schedule = {};
    fetchFromCameras<ScheduleTasks>(state.recording.schedule, cameras, calculateRecordingSchedule);
    const auto& schedule = state.recording.schedule;
    if (schedule.hasValue() && !schedule().empty())
    {
        const auto tasks = schedule();
        state.recording.brush.fps = std::max_element(tasks.cbegin(), tasks.cend(),
            [](const auto& l, const auto& r) { return l.fps < r.fps; })->fps;
    }
    else
    {
        state.recording.brush.fps = state.maxRecordingBrushFps();
    }

    fetchFromCameras<int>(state.recording.thresholds.beforeSec, cameras,
        calculateRecordingThresholdBefore);
    fetchFromCameras<int>(state.recording.thresholds.afterSec, cameras,
        calculateRecordingThresholdAfter);

    state.recording.brush.fps = qBound(
        kMinFps,
        state.recording.brush.fps,
        state.maxRecordingBrushFps());
    state = fillBitrateFromFixedQuality(std::move(state));

    state.recording.minDays = calculateMinRecordingDays(cameras);
    state.recording.maxDays = calculateMaxRecordingDays(cameras);

    state.imageControl = calculateImageControlSettings(cameras);

    fetchFromCameras<bool>(state.expert.dualStreamingDisabled, cameras,
        [](const Camera& camera) { return camera->isDualStreamingDisabled(); });
    fetchFromCameras<bool>(state.expert.cameraControlDisabled, cameras,
        [](const Camera& camera) { return camera->isCameraControlDisabled(); });

    fetchFromCameras<bool, Qn::BitratePerGopType>(state.expert.useBitratePerGOP, cameras,
        [](const Camera& camera) { return camera->bitratePerGopType(); },
        [](Qn::BitratePerGopType bpg) { return bpg != Qn::BPG_None; });

    fetchFromCameras<bool>(state.expert.primaryRecordingDisabled, cameras,
        [](const Camera& camera)
        {
            return camera->getProperty(QnMediaResource::dontRecordPrimaryStreamKey()).toInt() > 0;
        });

    fetchFromCameras<bool>(state.expert.secondaryRecordingDisabled, cameras,
        [](const Camera& camera)
        {
            return camera->getProperty(QnMediaResource::dontRecordSecondaryStreamKey()).toInt() > 0;
        });

    fetchFromCameras<vms::api::RtpTransportType>(state.expert.rtpTransportType, cameras,
        [](const Camera& camera)
        {
            return QnLexical::deserialized<vms::api::RtpTransportType>(
                camera->getProperty(QnMediaResource::rtpTransportKey()),
                vms::api::RtpTransportType::automatic);
        });

    fetchFromCameras<vms::api::MotionStreamType>(state.expert.motionStreamType, cameras,
        [](const Camera& camera) { return motionStreamType(camera); });

    state.expert.motionStreamOverridden = combinedValue(cameras,
        [](const Camera& camera)
        {
            return motionStreamType(camera) != vms::api::MotionStreamType::automatic;
        });

    if (state.devicesDescription.canDisableNativePtzPresets != State::CombinedValue::None)
    {
        fetchFromCameras<bool>(state.expert.nativePtzPresetsDisabled, cameras,
            [](const Camera& camera)
            {
                return camera->canDisableNativePtzPresets()
                    & !camera->getProperty(Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME).isEmpty();
            });
    }

    state.isDefaultExpertSettings = isDefaultExpertSettings(state);

    if (state.devicesDescription.isWearable == State::CombinedValue::All)
    {
        fetchFromCameras<bool>(state.wearableMotion.enabled, cameras,
            [](const Camera& camera)
            {
                NX_ASSERT(camera->getDefaultMotionType() == vms::api::MT_SoftwareGrid);
                return camera->getMotionType() == vms::api::MT_SoftwareGrid;
            });

        fetchFromCameras<int>(state.wearableMotion.sensitivity, cameras,
            [](const Camera& camera)
            {
                NX_ASSERT(camera->getVideoLayout()->channelCount() == 0);
                QnMotionRegion region = camera->getMotionRegion(0);
                const auto rects = region.getAllMotionRects();
                return rects.empty()
                    ? QnMotionRegion::kDefaultSensitivity
                    : rects.begin().key();
            });
    }

    return state;
}

State CameraSettingsDialogStateReducer::setSingleCameraUserName(State state, const QString& text)
{
    state.hasChanges = true;
    state.singleCameraProperties.name.setUser(text);
    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrush(
    State state,
    const ScheduleCellParams& brush)
{
    state.recording.brush = brush;
    const auto fps = qBound(
        kMinFps,
        state.recording.brush.fps,
        state.maxRecordingBrushFps());

    state = setScheduleBrushFps(std::move(state), fps);
    state.alert = State::Alert::brushChanged;

    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushRecordingType(
    State state,
    Qn::RecordingType value)
{
    NX_EXPECT(value != Qn::RecordingType::motionOnly || state.hasMotion());
    NX_EXPECT(value != Qn::RecordingType::motionAndLow
        || (state.hasMotion()
            && state.devicesDescription.hasDualStreamingCapability == State::CombinedValue::All));

    state.recording.brush.recordingType = value;
    if (value == Qn::RecordingType::motionAndLow)
    {
        state.recording.brush.fps = qBound(
            kMinFps,
            state.recording.brush.fps,
            state.maxRecordingBrushFps());
    }
    state.alert = State::Alert::brushChanged;

    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushFps(State state, int value)
{
    NX_EXPECT(qBound(kMinFps, value, state.maxRecordingBrushFps()) == value);
    state.recording.brush.fps = value;
    if (state.recording.brush.isAutomaticBitrate() || !state.recording.customBitrateAvailable)
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
        state = setCustomRecordingBitrateNormalized(std::move(state), normalizedBitrate);
    }
    state.alert = State::Alert::brushChanged;

    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushQuality(
    State state,
    Qn::StreamQuality value)
{
    state.recording.brush.quality = value;
    state = fillBitrateFromFixedQuality(std::move(state));
    state.alert = State::Alert::brushChanged;

    return state;
}

State CameraSettingsDialogStateReducer::setSchedule(State state, const ScheduleTasks& schedule)
{
    state.hasChanges = true;

    ScheduleTasks processed = schedule;
    if (!state.recording.customBitrateAvailable)
    {
        for (auto& task: processed)
            task.bitrateKbps = 0;
    }

    state.recording.schedule.setUser(processed);

    if (state.alert == State::Alert::brushChanged || state.alert == State::Alert::emptySchedule)
        state.alert = {};

    return state;
}

State CameraSettingsDialogStateReducer::setRecordingShowFps(State state, bool value)
{
    state.recording.showFps = value;
    return state;
}

State CameraSettingsDialogStateReducer::setRecordingShowQuality(State state, bool value)
{
    state.recording.showQuality = value;
    return state;
}

State CameraSettingsDialogStateReducer::toggleCustomBitrateVisible(State state)
{
    NX_EXPECT(state.recording.customBitrateAvailable);
    state.recording.customBitrateVisible = !state.recording.customBitrateVisible;
    return state;
}

State CameraSettingsDialogStateReducer::setCustomRecordingBitrateMbps(State state, float mbps)
{
    NX_EXPECT(state.recording.customBitrateAvailable && state.recording.customBitrateVisible);
    state.recording.brush.bitrateMbps = mbps;
    state.recording.brush.quality = calculateQualityForBitrateMbps(state, mbps);
    state.recording.bitrateMbps = mbps;
    return state;
}

State CameraSettingsDialogStateReducer::setCustomRecordingBitrateNormalized(
    State state,
    float value)
{
    NX_EXPECT(state.recording.customBitrateAvailable && state.recording.customBitrateVisible);
    const auto spread = state.recording.maxBitrateMpbs - state.recording.minBitrateMbps;
    const auto mbps = state.recording.minBitrateMbps + value * spread;
    return setCustomRecordingBitrateMbps(std::move(state), mbps);
}

State CameraSettingsDialogStateReducer::setMinRecordingDaysAutomatic(State state, bool value)
{
    state.hasChanges = true;
    state.recording.minDays.same = true;
    state.recording.minDays.automatic = value;
    return state;
}

State CameraSettingsDialogStateReducer::setMinRecordingDaysValue(State state, int value)
{
    NX_EXPECT(state.recording.minDays.same && !state.recording.minDays.automatic);
    state.hasChanges = true;
    state.recording.minDays.absoluteValue = value;
    return state;
}

State CameraSettingsDialogStateReducer::setMaxRecordingDaysAutomatic(State state, bool value)
{
    state.hasChanges = true;
    state.recording.maxDays.same = true;
    state.recording.maxDays.automatic = value;
    return state;
}

State CameraSettingsDialogStateReducer::setMaxRecordingDaysValue(State state, int value)
{
    NX_EXPECT(state.recording.maxDays.same && !state.recording.maxDays.automatic);
    state.hasChanges = true;
    state.recording.maxDays.absoluteValue = value;
    return state;
}

State CameraSettingsDialogStateReducer::setRecordingBeforeThresholdSec(State state, int value)
{
    NX_EXPECT(state.hasMotion());
    state.hasChanges = true;
    state.recording.thresholds.beforeSec.setUser(value);
    return state;
}

State CameraSettingsDialogStateReducer::setRecordingAfterThresholdSec(State state, int value)
{
    NX_EXPECT(state.hasMotion());
    state.hasChanges = true;
    state.recording.thresholds.afterSec.setUser(value);
    return state;
}

State CameraSettingsDialogStateReducer::setCustomAspectRatio(
    State state,
    const QnAspectRatio& value)
{
    NX_EXPECT(state.imageControl.aspectRatioAvailable);
    state.hasChanges = true;
    state.imageControl.aspectRatio.setUser(value);
    return state;
}

State CameraSettingsDialogStateReducer::setCustomRotation(State state, const Rotation& value)
{
    NX_EXPECT(state.imageControl.rotationAvailable);
    state.hasChanges = true;
    state.imageControl.rotation.setUser(value);
    return state;
}

State CameraSettingsDialogStateReducer::setRecordingEnabled(State state, bool value)
{
    state.hasChanges = true;
    state.recording.enabled.setUser(value);

    if (value && !state.recording.schedule.hasValue())
    {
        ScheduleTasks tasks;
        for (int dayOfWeek = 1; dayOfWeek <= 7; ++dayOfWeek)
        {
            QnScheduleTask data;
            data.dayOfWeek = dayOfWeek;
            data.startTime = 0;
            data.endTime = 86400;
            tasks << data;
        }
        state.recording.schedule.setUser(tasks);
    }

    return state;
}

State CameraSettingsDialogStateReducer::setMotionDetectionEnabled(State state, bool value)
{
    state.hasChanges = true;
    state.singleCameraSettings.enableMotionDetection.setUser(value);

    if (state.hasMotion() && !state.recording.enabled())
        state.alert = State::Alert::motionDetectionRequiresRecording;
    else if (state.alert == State::Alert::motionDetectionRequiresRecording)
        state.alert = {};

    return state;
}

State CameraSettingsDialogStateReducer::setMotionRegionList(
    State state, const QList<QnMotionRegion>& value)
{
    const auto errorCode = validateMotionRegionList(state, value);
    if (errorCode != QnMotionRegion::ErrorCode::Ok)
    {
        switch (errorCode)
        {
            case QnMotionRegion::ErrorCode::Windows:
                state.alert = State::Alert::motionDetectionTooManyRectangles;
                break;
            case QnMotionRegion::ErrorCode::Masks:
                state.alert = State::Alert::motionDetectionTooManyMaskRectangles;
                break;
            case QnMotionRegion::ErrorCode::Sens:
                state.alert = State::Alert::motionDetectionTooManySensitivityRectangles;
                break;
        }

        return state; //< Do not update region set.
    }

    state.hasChanges = true;
    state.singleCameraSettings.motionRegionList.setUser(value);
    return state;
}

State CameraSettingsDialogStateReducer::setFisheyeSettings(
    State state, const QnMediaDewarpingParams& value)
{
    state.singleCameraSettings.enableFisheyeDewarping.setUserIfChanged(value.enabled);
    state.singleCameraSettings.fisheyeMountingType.setUserIfChanged(value.viewMode);
    state.singleCameraSettings.fisheyeFovRotation.setUserIfChanged(value.fovRot);
    state.singleCameraSettings.fisheyeCalibrationSettings.setUserIfChanged(
        fisheyeCalibrationSettings(value));

    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setIoPortDataList(
    State state, const QnIOPortDataList& value)
{
    if (!state.isSingleCamera() || state.devicesDescription.isIoModule != State::CombinedValue::All)
        return state;

    state.singleIoModuleSettings.ioPortsData.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setIoModuleVisualStyle(
    State state, vms::api::IoModuleVisualStyle value)
{
    if (!state.isSingleCamera() || state.devicesDescription.isIoModule != State::CombinedValue::All)
        return state;

    state.singleIoModuleSettings.visualStyle.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setCameraControlDisabled(State state, bool value)
{
    const bool hasArecontCameras =
        state.devicesDescription.isArecontCamera != State::CombinedValue::None;

    if (hasArecontCameras || !state.settingsOptimizationEnabled)
        return state;

    state.expert.cameraControlDisabled.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setDualStreamingDisabled(State state, bool value)
{
    if (!state.settingsOptimizationEnabled)
        return state;

    state.expert.dualStreamingDisabled.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setUseBitratePerGOP(State state, bool value)
{
    const bool hasPredefinedBitratePerGOP =
        state.devicesDescription.hasPredefinedBitratePerGOP != State::CombinedValue::None;

    if (hasPredefinedBitratePerGOP || !state.settingsOptimizationEnabled)
        return state;

    state.expert.useBitratePerGOP.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setPrimaryRecordingDisabled(State state, bool value)
{
    state.expert.primaryRecordingDisabled.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setSecondaryRecordingDisabled(State state, bool value)
{
    if (state.devicesDescription.hasDualStreamingCapability == State::CombinedValue::None)
        return state;

    state.expert.secondaryRecordingDisabled.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setNativePtzPresetsDisabled(State state, bool value)
{
    if (state.devicesDescription.canDisableNativePtzPresets == State::CombinedValue::None)
        return state;

    state.expert.nativePtzPresetsDisabled.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setRtpTransportType(
    State state, vms::api::RtpTransportType value)
{
    state.expert.rtpTransportType.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setMotionStreamType(
    State state, vms::api::MotionStreamType value)
{
    if (state.devicesDescription.supportsMotionStreamOverride != State::CombinedValue::All)
        return state;

    state.expert.motionStreamType.setUser(value);
    state.expert.motionStreamOverridden = value == vms::api::MotionStreamType::automatic
        ? State::CombinedValue::None
        : State::CombinedValue::All;

    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setLogicalId(State state, int value)
{
    if (!state.isSingleCamera())
        return state;

    state.singleCameraSettings.logicalId.setUser(value);
    state.isDefaultExpertSettings = isDefaultExpertSettings(state);
    state.hasChanges = true;

    return updateDuplicateLogicalIdInfo(std::move(state));
}

State CameraSettingsDialogStateReducer::generateLogicalId(State state)
{
    if (!state.isSingleCamera())
        return state;

    auto cameras = qnClientCoreModule->commonModule()->resourcePool()->getAllCameras();
    std::set<int> usedValues;

    const QnUuid currentId(state.singleCameraProperties.id);
    for (const auto& camera: cameras)
    {
        if (camera->getId() != currentId)
            usedValues.insert(camera->getLogicalId().toInt());
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
    if (state.isDefaultExpertSettings)
        return state;

    state = setCameraControlDisabled(std::move(state), false);
    state = setDualStreamingDisabled(std::move(state), false);
    state = setUseBitratePerGOP(std::move(state), false);
    state = setPrimaryRecordingDisabled(std::move(state), false);
    state = setSecondaryRecordingDisabled(std::move(state), false);
    state = setNativePtzPresetsDisabled(std::move(state), false);
    state = setRtpTransportType(std::move(state), vms::api::RtpTransportType::automatic);
    state = setMotionStreamType(std::move(state), vms::api::MotionStreamType::automatic);
    state = setLogicalId(std::move(state), {});

    state.isDefaultExpertSettings = true;
    return state;
}

State CameraSettingsDialogStateReducer::setWearableMotionDetectionEnabled(State state, bool value)
{
    if (state.devicesDescription.isWearable != State::CombinedValue::All)
        return state;

    state.wearableMotion.enabled.setUser(value);
    state.hasChanges = true;
    return state;
}

State CameraSettingsDialogStateReducer::setWearableMotionSensitivity(State state, int value)
{
    if (state.devicesDescription.isWearable != State::CombinedValue::All)
        return state;

    state.wearableMotion.sensitivity.setUser(value);
    state.hasChanges = true;
    return state;
}

} // namespace desktop
} // namespace client
} // namespace nx
