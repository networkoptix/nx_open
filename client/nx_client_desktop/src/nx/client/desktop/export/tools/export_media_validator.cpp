#include "export_media_validator.h"

#include <client/client_module.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <camera/camera_data_manager.h>
#include <camera/loaders/caching_camera_data_loader.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <nx/core/transcoding/filters/filter_chain.h>
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
    const auto exportSpeed = qMax(1ll, settings.timelapseFrameStepMs
        / kTimelapseBaseFrameStepMs);

    // TODO: #GDM implement more precise estimation
    return durationMs / exportSpeed <= kMaxRecordingDurationMs;
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

QSize estimatedResolution(const QnMediaResourcePtr& mediaResource)
{
    if (const auto camera = mediaResource.dynamicCast<QnVirtualCameraResource>())
        return camera->defaultStream().getResolution();
    return QSize();
}

static const QSize kMaximumResolution(std::numeric_limits<int>::max(),
    std::numeric_limits<int>::max());

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

    if (FileExtensionUtils::isExecutable(settings.fileName.extension))
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

    core::transcoding::FilterChain filters(settings.transcodingSettings);

    if (filters.isTranscodingRequired(settings.mediaResource))
    {
        results.set(int(Result::transcoding));
        const auto resolution = estimatedResolution(settings.mediaResource);
        if (!resolution.isEmpty())
        {
            filters.prepare(settings.mediaResource, resolution, kMaximumResolution);
            if (filters.isDownscaleRequired(resolution))
                results.set(int(Result::downscaling));
        }
    }

    return results;
}

ExportMediaValidator::Results ExportMediaValidator::validateSettings(
    const ExportLayoutSettings& settings)
{
    Results results;

    const auto resPool = qnClientCoreModule->commonModule()->resourcePool();
    for (const auto& item : settings.layout->getItems())
    {
        const auto resource = resPool->getResourceByDescriptor(item.resource);
        if (resource && !resource.dynamicCast<QnVirtualCameraResource>())
        {
            results.set(int(Result::nonCameraResources));
            return results; //< This is a blocking alert. No other validation is required.
        }
    }

    if (FileExtensionUtils::isExecutable(settings.filename.extension))
    {
        // TODO: #GDM estimated binary size for layout.
        //if (exeFileIsTooBig(settings.layout, durationMs))
        //    results.set(int(Result::tooBigExeFile));
    }

    // Rough estimation.
    if (settings.period.durationMs * settings.layout->getItems().size() > 1000 * 60 * 30)
        results.set(int(Result::tooLong));

    return results;
}

} // namespace desktop
} // namespace client
} // namespace nx
