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

#include <nx/fusion/model_functions.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

// Maximum sane duration: 30 minutes of recording.
static constexpr qint64 kMaxRecordingDurationMs = 1000 * 60 * 30;

// Magic knowledge. We know that the result will be generated with 30 fps. --rvasilenko
static constexpr int kRapidReviewFps = 30;
static constexpr qint64 kTimelapseBaseFrameStepMs = 1000 / kRapidReviewFps;

// Default bitrate for exported file size estimate in megabytes per second.
static constexpr qreal kDefaultBitrateMBps = 0.5;

// Exe files greater than 4 Gb are not working.
static constexpr qint64 kMaximimumExeFileSizeMb = 4096;

// Reserving 200 Mb for client binaries.
static constexpr qint64 kReservedClientSizeMb = 200;

static constexpr int kMsPerSecond = 1000;

static constexpr int kBitsPerByte = 8;

bool validateLength(const ExportMediaSettings& settings, qint64 durationMs)
{
    const auto exportSpeed = qMax(1ll, settings.timelapseFrameStepMs
        / kTimelapseBaseFrameStepMs);

    // TODO: #GDM #Future implement more precise estimation
    return durationMs / exportSpeed <= kMaxRecordingDurationMs;
}

/** Calculate estimated video size in megabytes. */
qint64 estimatedExportVideoSizeMb(const QnMediaResourcePtr& mediaResource, qint64 durationMs)
{
    qreal maxBitrateMBps = kDefaultBitrateMBps;

    if (const auto camera = mediaResource.dynamicCast<QnVirtualCameraResource>())
    {
        //TODO: #GDM #3.2 Cache bitrates in resource
        auto bitrateInfos = QJson::deserialized<CameraBitrates>(
            camera->getProperty(Qn::CAMERA_BITRATE_INFO_LIST_PARAM_NAME).toUtf8());

        if (!bitrateInfos.streams.empty())
        {
            maxBitrateMBps = std::max_element(
                bitrateInfos.streams.cbegin(), bitrateInfos.streams.cend(),
                [](const CameraBitrateInfo& l, const CameraBitrateInfo &r)
                {
                    return l.actualBitrate < r.actualBitrate;
                })->actualBitrate;
        }
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

    if (FileExtensionUtils::isLayout(settings.fileName.extension))
    {
        results.set(int(Result::transcodingInLayoutIsNotSupported));
        if (FileExtensionUtils::isExecutable(settings.fileName.extension)
            && exeFileIsTooBig(settings.mediaResource, durationMs))
        {
            results.set(int(Result::tooBigExeFile));
        }
        return results;
    }

    if (settings.fileName.extension == FileExtension::avi && periods.size() > 1)
        results.set(int(Result::nonContinuosAvi));

    if (!validateLength(settings, durationMs))
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

    const auto isExecutable = FileExtensionUtils::isExecutable(settings.filename.extension);
    qint64 totalDurationMs = 0;
    qint64 estimatedTotalSizeMb = 0;

    const auto resPool = qnClientCoreModule->commonModule()->resourcePool();
    for (const auto& item: settings.layout->getItems())
    {
        const auto resource = resPool->getResourceByDescriptor(item.resource);
        if (!resource)
            continue;

        if (resource->hasFlags(Qn::still_image))
        {
            results.set(int(Result::nonCameraResources));
            continue;
        }

        const auto mediaResource = resource.dynamicCast<QnMediaResource>();
        if (!mediaResource)
        {
            results.set(int(Result::nonCameraResources));
            continue;
        }

        const auto loader = qnClientModule->cameraDataManager()->loader(mediaResource, false);
        const auto durationMs = loader
            ? loader->periods(Qn::RecordingContent).intersected(settings.period).duration()
            : settings.period.durationMs;

        totalDurationMs += durationMs;
        if (isExecutable)
            estimatedTotalSizeMb += estimatedExportVideoSizeMb(mediaResource, durationMs);
    }

    if (totalDurationMs > kMaxRecordingDurationMs)
        results.set(int(Result::tooLong));

    if (isExecutable && estimatedTotalSizeMb + kReservedClientSizeMb > kMaximimumExeFileSizeMb)
        results.set(int(Result::tooBigExeFile));

    return results;
}

} // namespace desktop
} // namespace client
} // namespace nx
