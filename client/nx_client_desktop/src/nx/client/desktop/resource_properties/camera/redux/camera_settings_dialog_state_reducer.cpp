#include "camera_settings_dialog_state_reducer.h"

#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <camera/fps_calculator.h>
#include <utils/camera/camera_bitrate_calculator.h>

namespace nx {
namespace client {
namespace desktop {

class CameraSettingsDialogStateStrings
{
    Q_DECLARE_TR_FUNCTIONS(CameraSettingsDialogStateStrings)
};

using State = CameraSettingsDialogState;

namespace {

static constexpr int kMinFps = 1;

static constexpr auto kMinQuality = Qn::QualityLow;
static constexpr auto kMaxQuality = Qn::QualityHighest;

QString calculateWebPage(const QnVirtualCameraResourcePtr& camera)
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

bool calculateRecordingParametersAvailable(const QnVirtualCameraResourceList& cameras)
{
    return std::any_of(
        cameras.cbegin(),
        cameras.cend(),
        [](const QnVirtualCameraResourcePtr& camera)
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
    state.recording.brush.bitrateMbps = QnScheduleGridWidget::CellParams::kAutomaticBitrate;
    state.recording.bitrateMbps = calculateBitrateForQualityMbps(
        state,
        state.recording.brush.quality);
    return state;
}

State loadNetworkInfo(State state, const QnVirtualCameraResourcePtr& camera)
{
    NX_ASSERT(camera);
    if (!camera)
        return state;

    state.singleCameraSettings.ipAddress = QnResourceDisplayInfo(camera).host();
    state.singleCameraSettings.webPage = calculateWebPage(camera);

    const bool isIoModule = camera->isIOModule();
    const bool hasPrimaryStream = !isIoModule || camera->isAudioSupported();
    state.singleCameraSettings.primaryStream = hasPrimaryStream
        ? camera->sourceUrl(Qn::CR_LiveVideo)
        : CameraSettingsDialogStateStrings::tr("I/O module has no audio stream");

    const bool hasSecondaryStream = camera->hasDualStreaming();
    state.singleCameraSettings.secondaryStream = hasSecondaryStream
        ? camera->sourceUrl(Qn::CR_SecondaryLiveVideo)
        : CameraSettingsDialogStateStrings::tr("Camera has no secondary stream");

    return state;
}

State::RecordingDays calculateMinRecordingDays(const QnVirtualCameraResourceList& cameras)
{
    if (cameras.empty())
        return {ec2::kDefaultMinArchiveDays, true, true};

    // Any negative min days value means 'auto'. Storing absolute value to keep previous one.
    auto calcMinDays = [](int d) { return d == 0 ? ec2::kDefaultMinArchiveDays : qAbs(d); };

    const int minDays = (*std::min_element(
        cameras.cbegin(),
        cameras.cend(),
        [calcMinDays](
        const QnVirtualCameraResourcePtr& l,
        const QnVirtualCameraResourcePtr& r)
        {
            return calcMinDays(l->minDays()) < calcMinDays(r->minDays());
        }))->minDays();

    const bool isAuto = minDays <= 0;
    const bool sameMinDays = std::all_of(
        cameras.cbegin(),
        cameras.cend(),
        [minDays, isAuto](const QnVirtualCameraResourcePtr& camera)
        {
            return isAuto
                ? camera->minDays() <= 0
                : camera->minDays() == minDays;
        });
    return {calcMinDays(minDays), isAuto, sameMinDays};
}

State::RecordingDays calculateMaxRecordingDays(const QnVirtualCameraResourceList& cameras)
{
    if (cameras.empty())
        return {ec2::kDefaultMaxArchiveDays, true, true};

    /* Any negative max days value means 'auto'. Storing absolute value to keep previous one. */
    auto calcMaxDays = [](int d) { return d == 0 ? ec2::kDefaultMaxArchiveDays : qAbs(d); };

    const int maxDays = (*std::max_element(
        cameras.cbegin(),
        cameras.cend(),
        [calcMaxDays](
        const QnVirtualCameraResourcePtr& l,
        const QnVirtualCameraResourcePtr& r)
        {
            return calcMaxDays(l->maxDays()) < calcMaxDays(r->maxDays());
        }))->maxDays();

    const bool isAuto = maxDays <= 0;
    const bool sameMaxDays = std::all_of(
        cameras.cbegin(),
        cameras.cend(),
        [maxDays, isAuto](const QnVirtualCameraResourcePtr& camera)
        {
            return isAuto
                ? camera->maxDays() <= 0
                : camera->maxDays() == maxDays;
        });
    return {calcMaxDays(maxDays), isAuto, sameMaxDays};
}

} // namespace

State CameraSettingsDialogStateReducer::applyChanges(State state)
{
    state.hasChanges = false;
    state.singleCameraSettings.name.submitToBase();

    return state;
}

State CameraSettingsDialogStateReducer::loadCameras(
    State state,
    const QnVirtualCameraResourceList& cameras)
{
    const auto firstCamera = cameras.empty()
        ? QnVirtualCameraResourcePtr()
        : cameras.first();

    state.hasChanges = false;
    state.singleCameraSettings = CameraSettingsDialogState::SingleCameraSettings();
    state.recording = {};
    state.devicesCount = cameras.size();

    if (firstCamera)
    {
        state.singleCameraSettings.name.setBase(firstCamera->getName());
        state.singleCameraSettings.id = firstCamera->getId().toSimpleString();
        state.singleCameraSettings.firmware = firstCamera->getFirmware();
        state.singleCameraSettings.macAddress = firstCamera->getMAC().toString();
        state.singleCameraSettings.model = firstCamera->getModel();
        state.singleCameraSettings.vendor = firstCamera->getVendor();

        state = loadNetworkInfo(state, firstCamera);

        state.recording.bitratePerGopType = firstCamera->bitratePerGopType();
        state.recording.defaultStreamResolution = firstCamera->streamInfo().getResolution();
        state.recording.mediaStreamCapability = firstCamera->cameraMediaCapability().
            streamCapabilities.value(Qn::StreamIndex::primary);

        state.recording.customBitrateAvailable = true;
        state = loadMinMaxCustomBitrate(state);
    }

    state.recording.parametersAvailable = calculateRecordingParametersAvailable(cameras);

    // TODO: Handle overriding motion type.
    const auto fpsLimits = Qn::calculateMaxFps(cameras);
    state.recording.maxFps = qMax(kMinFps, fpsLimits.first);
    state.recording.maxDualStreamingFps = qMax(kMinFps, fpsLimits.second);
    state.recording.brush.fps = qBound(
        kMinFps,
        state.recording.brush.fps,
        state.recording.maxBrushFps());
    state = fillBitrateFromFixedQuality(state);

    state.recording.minDays = calculateMinRecordingDays(cameras);
    state.recording.maxDays = calculateMaxRecordingDays(cameras);

    return state;
}

State CameraSettingsDialogStateReducer::setSingleCameraUserName(State state, const QString& text)
{
    state.hasChanges = true;
    state.singleCameraSettings.name.setUser(text);
    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushRecordingType(
    State state,
    Qn::RecordingType value)
{
    state.recording.brush.recordingType = value;
    if (value == Qn::RT_MotionAndLowQuality)
    {
        state.recording.brush.fps = qBound(
            kMinFps,
            state.recording.brush.fps,
            state.recording.maxBrushFps());
    }
    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushFps(State state, int value)
{
    NX_EXPECT(qBound(kMinFps, value, state.recording.maxBrushFps()) == value);
    state.recording.brush.fps = value;
    if (state.recording.brush.isAutomaticBitrate() || !state.recording.customBitrateAvailable)
    {
        // Lock quality.
        state = loadMinMaxCustomBitrate(state);
        state = fillBitrateFromFixedQuality(state);
    }
    else
    {
        // Lock normalized bitrate.
        const auto normalizedBitrate = state.recording.normalizedCustomBitrateMbps();
        state = loadMinMaxCustomBitrate(state);
        state = setCustomRecordingBitrateNormalized(state, normalizedBitrate);
    }

    return state;
}

State CameraSettingsDialogStateReducer::setScheduleBrushQuality(
    State state,
    Qn::StreamQuality value)
{
    state.recording.brush.quality = value;
    state = fillBitrateFromFixedQuality(state);
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
    return setCustomRecordingBitrateMbps(state, mbps);
}

State CameraSettingsDialogStateReducer::setMinRecordingDaysAutomatic(State state, bool value)
{
    state.recording.minDays.same = true;
    state.recording.minDays.automatic = value;
    return state;
}

State CameraSettingsDialogStateReducer::setMinRecordingDaysValue(State state, int value)
{
    NX_EXPECT(state.recording.minDays.same && !state.recording.minDays.automatic);
    state.recording.minDays.absoluteValue = value;
    return state;
}

State CameraSettingsDialogStateReducer::setMaxRecordingDaysAutomatic(State state, bool value)
{
    state.recording.maxDays.same = true;
    state.recording.maxDays.automatic = value;
    return state;
}

State CameraSettingsDialogStateReducer::setMaxRecordingDaysValue(State state, int value)
{
    NX_EXPECT(state.recording.maxDays.same && !state.recording.maxDays.automatic);
    state.recording.maxDays.absoluteValue = value;
    return state;
}

} // namespace desktop
} // namespace client
} // namespace nx
