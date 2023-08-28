// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_media_validator.h"

#include <camera/camera_data_manager.h>
#include <client_core/client_core_module.h>
#include <core/resource/camera_media_stream_info.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/transcoding/filters/filter_chain.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/desktop/common/utils/filesystem.h>
#include <nx/vms/client/desktop/export/data/export_layout_settings.h>
#include <nx/vms/client/desktop/export/data/export_media_settings.h>
#include <nx/vms/client/desktop/layout/layout_data_helper.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>

namespace nx::vms::client::desktop {

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

    // TODO: #sivanov Implement more precise estimation.
    return durationMs / exportSpeed <= kMaxRecordingDurationMs;
}

/** Calculate estimated video size in megabytes. */
qint64 estimatedExportVideoSizeMb(const QnMediaResourcePtr& mediaResource, qint64 durationMs)
{
    qreal maxBitrateMBps = kDefaultBitrateMBps;

    if (const auto camera = mediaResource.dynamicCast<QnVirtualCameraResource>())
    {
        //TODO: #sivanov Cache bitrates in resource.
        auto bitrateInfos = QJson::deserialized<CameraBitrates>(
            camera->getProperty(ResourcePropertyKey::kBitrateInfos).toUtf8());

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

QSize estimatedResolution(const QnMediaResourcePtr& mediaResource)
{
    if (const auto camera = mediaResource.dynamicCast<QnVirtualCameraResource>())
        return camera->streamInfo().getResolution();
    return QSize();
}

static const QSize kMaximumResolution(std::numeric_limits<int>::max(),
    std::numeric_limits<int>::max());

} // namespace

ExportMediaValidator::Results ExportMediaValidator::validateSettings(
    const ExportMediaSettings& settings, QnMediaResourcePtr mediaResource)
{
    const QnResourcePtr resource = mediaResource->toResourcePtr();
    auto systemContext = SystemContext::fromResource(resource);
    if (!NX_ASSERT(systemContext))
        return {};

    const auto loader = systemContext->cameraDataManager()->loader(
        mediaResource,
        /*createIfNotExists*/ false);

    const auto periods = loader
        ? loader->periods(Qn::RecordingContent).intersected(settings.period)
        : QnTimePeriodList();
    const auto durationMs = periods.isEmpty()
        ? settings.period.durationMs
        : periods.duration();

    Results results;

    if (!systemContext->accessController()->hasPermissions(resource, Qn::ExportPermission))
        results.set((int) Result::exportNotAllowed);

    // The local video file may not have timestamps.
    // This does not mean that there is no data in it. We can export it.

    const auto isLocalVideoFile = resource->hasFlags(Qn::local_video) && !resource->hasFlags(Qn::periods);

    if (periods.isEmpty() && !isLocalVideoFile)
        results.set(int(Result::noCameraData));

    if (FileExtensionUtils::isLayout(settings.fileName.extension))
    {
        results.set(int(Result::transcodingInLayoutIsNotSupported));
        if (FileExtensionUtils::isExecutable(settings.fileName.extension)
            && exeFileIsTooBig(mediaResource, durationMs))
        {
            results.set(int(Result::tooBigExeFile));
        }
        return results;
    }

    if (settings.fileName.extension == FileExtension::avi)
    {
        // Problems with audio track can occur even if the video is continuous. So we are displaying
        // a warning always when exporting to the avi file except for cameras without sound at all.
        const auto camera = mediaResource.dynamicCast<QnVirtualCameraResource>();
        if (!camera || camera->isAudioSupported())
            results.set(int(Result::aviWithAudio));
    }

    if (!validateLength(settings, durationMs))
        results.set(int(Result::tooLong));

    nx::core::transcoding::FilterChain filters(
        settings.transcodingSettings,
        mediaResource->getDewarpingParams(),
        mediaResource->getVideoLayout());

    if (filters.isTranscodingRequired())
    {
        results.set(int(Result::transcoding));
        const auto resolution = estimatedResolution(mediaResource);
        if (!resolution.isEmpty())
        {
            filters.prepare(resolution, kMaximumResolution);
            if (filters.isDownscaleRequired(resolution))
                results.set(int(Result::downscaling));
        }
    }

    return results;
}

ExportMediaValidator::Results ExportMediaValidator::validateSettings(
    const ExportLayoutSettings& settings, QnLayoutResourcePtr layout)
{
    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return {};

    Results results;

    const auto isExecutable = FileExtensionUtils::isExecutable(settings.fileName.extension);
    qint64 totalDurationMs = 0;
    qint64 estimatedTotalSizeMb = 0;

    for (const auto& item: layout->getItems())
    {
        const auto resource = getResourceByDescriptor(item.resource);
        if (!resource)
            continue;

        if (!systemContext->accessController()->hasPermissions(resource, Qn::ExportPermission))
        {
            results.set((int) Result::exportNotAllowed);
            continue;
        }

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

        auto systemContext = SystemContext::fromResource(resource);
        if (!NX_ASSERT(systemContext))
            continue;

        const auto loader = systemContext->cameraDataManager()->loader(mediaResource, false);
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

bool ExportMediaValidator::exeFileIsTooBig(const QnMediaResourcePtr& mediaResource, qint64 durationMs)
{
    const auto videoSizeMb = estimatedExportVideoSizeMb(mediaResource, durationMs);
    return (videoSizeMb + kReservedClientSizeMb > kMaximimumExeFileSizeMb);
}

bool ExportMediaValidator::exeFileIsTooBig(
    const QnLayoutResourcePtr& layout,
    qint64 durationMs)
{
    qint64 videoSizeMb = 0;
    const auto resources = layoutResources(layout);
    for (const auto& resource: resources)
    {
        if (const QnMediaResourcePtr& media = resource.dynamicCast<QnMediaResource>())
            videoSizeMb += estimatedExportVideoSizeMb(media, durationMs);
    }
    return (videoSizeMb + kReservedClientSizeMb > kMaximimumExeFileSizeMb);
}

} // namespace nx::vms::client::desktop
