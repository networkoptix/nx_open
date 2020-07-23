#include "analytics_db.h"

#include <nx/vms/server/root_fs.h>

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <media_server/media_server_module.h>
#include <plugins/resource/server_archive/server_archive_delegate.h>

namespace nx::vms::server::analytics {

AnalyticsDb::AnalyticsDb(
    QnMediaServerModule* mediaServerModule,
    nx::analytics::db::AbstractIframeSearchHelper* iframeSearchHelper,
    nx::analytics::db::AbstractObjectTypeDictionary* objectTypeDictionary)
    :
    base_type(mediaServerModule->commonModule(), iframeSearchHelper, objectTypeDictionary),
    m_mediaServerModule(mediaServerModule)
{
}

std::vector<nx::analytics::db::ObjectPosition> AnalyticsDb::lookupTrackDetailsSync(
    const nx::analytics::db::ObjectTrack& track)
{
    auto cachedTrack = base_type::lookupTrackDetailsSync(track);
    if (!cachedTrack.empty())
        return cachedTrack;

    nx::utils::ElapsedTimer timer;
    timer.restart();

    std::vector<nx::analytics::db::ObjectPosition> result;
    auto resource = m_mediaServerModule->commonModule()->resourcePool()->getResourceById(track.deviceId);
    if (!resource)
    {
        NX_DEBUG(this, "Failed to to lookup detail track info for deviceId: %1", track.deviceId);
        return result;
    }

    QnServerArchiveDelegate archive(m_mediaServerModule, MediaQuality::MEDIA_Quality_High);
    if (!archive.open(resource, m_mediaServerModule->archiveIntegrityWatcher()))
    {
        NX_DEBUG(this, "Failed to to lookup detail track info for objectTraclId: %1", track.id);
        return result;
    }
    archive.seek(track.firstAppearanceTimeUs, true);
    const auto lastTime = track.lastAppearanceTimeUs;
    while (auto data = std::dynamic_pointer_cast<QnAbstractMediaData>(archive.getNextData()))
    {
        if (data->timestamp > lastTime || data->dataType == QnAbstractMediaData::EMPTY_DATA)
            break;

        auto metadata = std::dynamic_pointer_cast<QnCompressedMetadata>(data);
        if (!metadata)
            continue;

        auto packet = nx::common::metadata::fromCompressedMetadataPacket(metadata);
        if (!packet)
            break;

        for (const auto& detailData : packet->objectMetadataList)
        {
            if (detailData.trackId == track.id)
            {
                nx::analytics::db::ObjectPosition position;
                position.deviceId = track.deviceId;
                position.timestampUs = packet->timestampUs;
                position.durationUs = packet->durationUs;
                position.attributes = detailData.attributes;
                position.boundingBox = detailData.boundingBox;
                result.emplace_back(position);
            }
        }
    }

    NX_VERBOSE(this, "track details data has read from media archive. Request duration %1",
        timer.elapsed());

    return result;
}

bool AnalyticsDb::makePath(const QString& path)
{
    if (!m_mediaServerModule->rootFileSystem()->makeDirectory(path))
    {
        NX_WARNING(this, "Failed to create directory %1", path);
        return false;
    }

    NX_DEBUG(this, "Directory %1 exists or has been created successfully", path);
    return true;
}

bool AnalyticsDb::makeWritable(const std::vector<PathAndMode>& pathAndModeList)
{
    for (const auto& entry: pathAndModeList)
    {
        const ChownMode mode = std::get<ChownMode>(entry);
        const QString path = std::get<QString>(entry);
        const auto rootFs = m_mediaServerModule->rootFileSystem();

        if (!rootFs->isPathExists(path))
        {
            NX_DEBUG(this, "Requested to change owner for a path %1, but it doesn't exist. Skipping.", path);
            continue;
        }

        if (mode == ChownMode::mountPoint)
        {
            // On linux write access on any location under the mount point requires read access on
            // the mount point itself.
            if (!rootFs->makeReadable(path))
            {
                NX_WARNING(this, "Unable to give read access to the mount point: %1", path);
                continue;
            }

            NX_DEBUG(this, "Read access is given to the mount point: %1", path);
        }
        else
        {
            if (!rootFs->changeOwner(path, mode == ChownMode::recursive))
            {
                NX_WARNING(this, "Failed to change access rights. Mode: %1. Path: %2", mode, path);
                return false;
            }

            NX_DEBUG(this, "Succeed to change access rights. Mode: %1. Path: %2", mode, path);
        }
    }

    return true;
}

} // namespace nx::vms::server::analytics
