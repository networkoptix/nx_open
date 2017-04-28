#pragma once

#include <QtCore/QObject>
#include <QtCore/QByteArray>

#include <vector>

#include <core/resource/resource_fwd.h>
#include <core/resource/abstract_remote_archive_manager.h>
#include <core/resource/resource_media_layout.h>
#include <server/server_globals.h>

namespace nx {
namespace mediaserver_core {
namespace recorder {

class RemoteArchiveSynchronizer: public QObject
{
    Q_OBJECT

    using AbstractRemoteArchiveManager = nx::core::resource::AbstractRemoteArchiveManager;
    using RemoteArchiveEntry = nx::core::resource::RemoteArchiveEntry;
    using BufferType = QByteArray;

public:
    RemoteArchiveSynchronizer();

public slots:
    void at_newResourceAdded(const QnResourcePtr& resource);
    void at_resourceInitializationChanged(const QnResourcePtr& resource);

private:
    bool synchronizeArchive(const QnSecurityCamResourcePtr& manager);

    std::vector<RemoteArchiveEntry> filterEntries(
        const QnSecurityCamResourcePtr& resource,
        const std::vector<RemoteArchiveEntry>& allEntries);

    bool moveEntryToArchive(
        const QnSecurityCamResourcePtr& resource,
        const RemoteArchiveEntry& entry);

    bool writeEntry(
        const QnSecurityCamResourcePtr& resource,
        const RemoteArchiveEntry& entry,
        BufferType* buffer);

    bool convertAndWriteBuffer(
        const QnSecurityCamResourcePtr& resource,
        BufferType* buffer,
        const QString& fileName,
        int64_t startTimeMs,
        const QnResourceAudioLayoutPtr& audioLayout,
        bool needMotion);

    QnServer::ChunksCatalog chunksCatalogByResolution(const QSize& resolution) const;

    bool isMotionDetectionNeeded(
        const QnSecurityCamResourcePtr& resource,
        QnServer::ChunksCatalog catalog) const;
};

} // namespace recorder
} // namespace mediaserver_core
} // namespace nx