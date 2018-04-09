#include "camera_settings_dialog_state_reducer.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <camera/fps_calculator.h>
#include <utils/camera/camera_bitrate_calculator.h>
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

static constexpr auto kMinQuality = Qn::QualityLow;
static constexpr auto kMaxQuality = Qn::QualityHighest;

template<class Data>
void fetchFromCameras(
    State::UserEditableMultiple<Data>& value,
    const Cameras& cameras,
    std::function<Data(const Camera&)> predicate)
{
    Data data;
    if (utils::algorithm::same(cameras.cbegin(), cameras.cend(), predicate, &data))
        value.setBase(data);
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

bool isMotionDetectionEnabled(const Camera& camera)
{
    const auto motionType = camera->getMotionType();
    return motionType != Qn::MT_NoMotion && camera->supportedMotionType().testFlag(motionType);
}

bool calculateRecordingParametersAvailable(const Cameras& cameras)
{
    return std::any_of(
        cameras.cbegin(),
        cameras.cend(),
        [](const Camera& camera)
        {
            return camera->hasVideo()
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

    for (int i = current + 1; i <= kMaxQuality; ++i)
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
    state.recording.minBitrateMbps = calculateBitrateForQualityMbps(state, Qn::QualityLowest);
    state.recording.maxBitrateMpbs = calculateBitrateForQualityMbps(state, Qn::QualityHighest);
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
        return {ec2::kDefaultMinArchiveDays, true, true};

    // Any negative min days value means 'auto'. Storing absolute value to keep previous one.
    auto calcMinDays = [](int d) { return d == 0 ? ec2::kDefaultMinArchiveDays : qAbs(d); };

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
        return {ec2::kDefaultMaxArchiveDays, true, true};

    /* Any negative max days value means 'auto'. Storing absolute value to keep previous one. */
    auto calcMaxDays = [](int d) { return d == 0 ? ec2::kDefaultMaxArchiveDays : qAbs(d); };

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
    result.aspectRatioAvailable = hasVideo;

    if (result.aspectRatioAvailable)
    {
        fetchFromCameras<QnAspectRatio>(
            result.aspectRatio,
            cameras,
            [](const auto& camera) { return camera->customAspectRatio(); });
    }

    result.rotationAvailable = hasVideo && std::all_of(cameras.cbegin(), cameras.cend(),
        [](const auto& camera) { return !camera->hasFlags(Qn::wearable_camera); });

    if (result.rotationAvailable)
    {
        fetchFromCameras<Rotation>(
            result.rotation,
            cameras,
            [](const auto& camera)
            {
                QString rotationString = camera->getProperty(QnMediaResource::rotationKey());
                return rotationString.isEmpty()
                    ? Rotation()
                    : Rotation::closestStandardRotation(rotationString.toInt());
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
    return value > 0 ? value : ec2::kDefaultRecordBeforeMotionSec;
}

int calculateRecordingThresholdAfter(const Camera& camera)
{
    const auto value = camera->recordAfterMotionSec();
    return value > 0 ? value : ec2::kDefaultRecordAfterMotionSec;
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

State CameraSettingsDialogStateReducer::setPanicMode(State state, bool value)
{
    state.panicMode = value;
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
    state.devicesDescription = {};
    state.recording = {};
    state.devicesCount = cameras.size();

    state.devicesDescription.isDtsBased = combinedValue(cameras,
        [](const Camera& camera) { return camera->isDtsBased(); });
    state.devicesDescription.isWearable = combinedValue(cameras,
        [](const Camera& camera) { return camera->hasFlags(Qn::wearable_camera); });
    state.devicesDescription.hasMotion = combinedValue(cameras,
        [](const Camera& camera) { return camera->hasMotion(); });
    state.devicesDescription.hasDualStreaming = combinedValue(cameras,
        [](const Camera& camera) { return camera->hasDualStreaming(); });

    if (firstCamera)
    {
        state.singleCameraProperties.name.setBase(firstCamera->getName());
        state.singleCameraProperties.id = firstCamera->getId().toSimpleString();
        state.singleCameraProperties.firmware = firstCamera->getFirmware();
        state.singleCameraProperties.macAddress = firstCamera->getMAC().toString();
        state.singleCameraProperties.model = firstCamera->getModel();
        state.singleCameraProperties.vendor = firstCamera->getVendor();

        state = loadNetworkInfo(std::move(state), firstCamera);

        state.recording.bitratePerGopType = firstCamera->bitratePerGopType();
        state.recording.defaultStreamResolution = firstCamera->streamInfo().getResolution();
        state.recording.mediaStreamCapability = firstCamera->cameraMediaCapability().
            streamCapabilities.value(Qn::StreamIndex::primary);

        state.recording.customBitrateAvailable = true;
        state = loadMinMaxCustomBitrate(std::move(state));

        state.singleCameraSettings.enableMotionDetection.setBase(
            isMotionDetectionEnabled(firstCamera));

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

    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushRecordingType(
    State state,
    Qn::RecordingType value)
{
    NX_EXPECT(value != Qn::RT_MotionOnly || state.hasMotion());
    NX_EXPECT(value != Qn::RT_MotionAndLowQuality || state.hasDualStreaming());
    state.recording.brush.recordingType = value;
    if (value == Qn::RT_MotionAndLowQuality)
    {
        state.recording.brush.fps = qBound(
            kMinFps,
            state.recording.brush.fps,
            state.maxRecordingBrushFps());
    }
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

    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushQuality(
    State state,
    Qn::StreamQuality value)
{
    state.recording.brush.quality = value;
    state = fillBitrateFromFixedQuality(std::move(state));
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

} // namespace desktop
} // namespace client
} // namespace nx
