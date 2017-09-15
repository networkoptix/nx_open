#include "export_media_validator.h"

#include <client/client_module.h>

#include <core/resource/camera_resource.h>

#include <camera/camera_data_manager.h>
#include <camera/loaders/caching_camera_data_loader.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <nx/client/desktop/common/utils/filesystem.h>
#include <nx/client/desktop/export/data/export_media_settings.h>
#include <nx/client/desktop/export/data/export_layout_settings.h>

// TODO: #GDM move constant to another place
#include <nx/client/desktop/ui/dialogs/rapid_review_dialog.h>

#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace desktop {

namespace
{

/** Maximum sane duration: 30 minutes of recording. */
static const qint64 kMaxRecordingDurationMs = 1000 * 60 * 30;
static const qint64 kTimelapseBaseFrameStepMs = 1000 / ui::dialogs::ExportRapidReview::kResultFps;

/** Default bitrate for exported file size estimate in megabytes per second. */
static const qreal kDefaultBitrateMBps = 0.5;

/** Exe files greater than 4 Gb are not working. */
static const qint64 kMaximimumExeFileSizeMb = 4096;

/** Reserving 200 Mb for client binaries. */
static const qint64 kReservedClientSizeMb = 200;

static const int kMsPerSecond = 1000;

static const int kBitsPerByte = 8;

bool validateLength(const ExportMediaSettings& settings, qint64 durationMs)
{
    const auto exportSpeed = qMax(1ll, settings.timelapseFrameStepMs / kTimelapseBaseFrameStepMs);

    // TODO: #GDM implement more precise estimation
    return durationMs / exportSpeed <= kMaxRecordingDurationMs;
}

bool isPanoramic(const QnMediaResourcePtr& mediaResource)
{
    const auto videoLayout = mediaResource->getVideoLayout();
    return videoLayout && videoLayout->channelCount() > 1;
}

/** Calculate estimated video size in megabytes. */
qint64 estimatedExportVideoSizeMb(const QnMediaResourcePtr& mediaResource, qint64 durationMs)
{
    qreal maxBitrateMBps = kDefaultBitrateMBps;

    if (const auto camera = mediaResource.dynamicCast<QnVirtualCameraResource>())
    {
        //TODO: #GDM Cache bitrates in resource
        auto bitrateInfos = QJson::deserialized<CameraBitrates>(
            camera->getProperty(Qn::CAMERA_BITRATE_INFO_LIST_PARAM_NAME).toUtf8());
        if (!bitrateInfos.streams.empty())
            maxBitrateMBps = std::max_element(
                bitrateInfos.streams.cbegin(), bitrateInfos.streams.cend(),
                [](const CameraBitrateInfo& l, const CameraBitrateInfo &r)
                {
                    return l.actualBitrate < r.actualBitrate;
                })->actualBitrate;
    }

    return maxBitrateMBps * durationMs / (kMsPerSecond * kBitsPerByte);
}

bool exeFileIsTooBig(const QnMediaResourcePtr& mediaResource, qint64 durationMs)
{
    const auto videoSizeMb = estimatedExportVideoSizeMb(mediaResource, durationMs);
    return (videoSizeMb + kReservedClientSizeMb > kMaximimumExeFileSizeMb);
}

} // namespace

ExportMediaValidator::Results ExportMediaValidator::validateSettings(
    const ExportMediaSettings& settings)
{
    const auto loader = qnClientModule->cameraDataManager()->loader(settings.mediaResource, false);
    const auto periods = loader
        ? loader->periods(Qn::RecordingContent).intersected(settings.timePeriod)
        : QnTimePeriodList();
    const auto durationMs = periods.isEmpty()
        ? settings.timePeriod.durationMs
        : periods.duration();

    Results results;

    if (!FileExtensionUtils::isExecutable(settings.fileName.extension))
    {
        results.set(int(Result::transcodingInBinaryIsNotSupported));
        if (exeFileIsTooBig(settings.mediaResource, durationMs))
            results.set(int(Result::tooBigExeFile));
        return results;
    }

    if (settings.fileName.extension == FileExtension::avi && periods.size() > 1)
        results.set(int(Result::nonContinuosAvi));

    if  (!validateLength(settings, durationMs))
        results.set(int(Result::tooLong));

    const bool transcodingRequired = settings.enhancement.enabled
        || settings.aspectRatio.isValid()
        || settings.rotation != 0
        || settings.dewarping.enabled
        || !settings.zoomWindow.isEmpty()
        || !settings.overlays.empty()
        || isPanoramic(settings.mediaResource);

    if (transcodingRequired)
    {
        results.set(int(Result::transcoding));
        if (const auto camera = settings.mediaResource.dynamicCast<QnVirtualCameraResource>())
        {
            const auto stream = camera->defaultStream();
            const auto resolution = stream.getResolution();

            if (!resolution.isEmpty())
            {
                const int bigValue = std::numeric_limits<int>::max();
                NX_ASSERT(resolution.isValid());

                //auto filters = imageParameters.createFilterChain(resolution, QSize(bigValue, bigValue));
                //const QSize resultResolution = imageParameters.updatedResolution(filters, resolution);
                //if (resultResolution.width() > imageParameters.defaultResolutionLimit.width() ||
                //    resultResolution.height() > imageParameters.defaultResolutionLimit.height())
                //{
                //    results.set(int(Result::downscaling));
                //}

            }
        }
    }

    return results;
}

ExportMediaValidator::Results ExportMediaValidator::validateSettings(
    const ExportLayoutSettings& settings)
{
    if (FileExtensionUtils::isExecutable(settings.filename.extension))
    {

    }

    return Results();
}

} // namespace desktop
} // namespace client
} // namespace nx
