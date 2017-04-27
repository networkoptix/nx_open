#pragma once

#include <vector>

#include <QtCore/QString>

#include <boost/optional/optional.hpp>

#include <nx/network/http/httpclient.h>
#include <plugins/resource/onvif/onvif_resource.h>
#include <plugins/common_interfaces/abstract_remote_archive_fetcher.h>
#include <server/server_globals.h>


namespace nx {
namespace mediaserver_core {
namespace plugins {

class LilinResource: 
    public QnPlOnvifResource,
    public AbstractRemoteArchiveFetcher
{
 
    using base_type = QnPlOnvifResource;

public:
    LilinResource();
    virtual ~LilinResource() override;

    virtual CameraDiagnostics::Result initInternal() override;

    // Implementation of AbstractRemoteArchiveFetcher::listAvailableArchiveEntries 
    virtual bool listAvailableArchiveEntries(
        std::vector<RemoteArchiveEntry>* outArchiveEntries,
        uint64_t startTimeMs = 0 ,
        uint64_t endTimeMs = std::numeric_limits<uint64_t>::max()) override;

    // Implementation of AbstractRemoteArchiveFetcher::fetchArchiveEntries
    virtual bool fetchArchiveEntry(const QString& entryId, BufferType* outBuffer) override;

    // Implementation of AbstractRemoteArchiveFetcher::removeArchiveEntry
    virtual bool removeArchiveEntries(const std::vector<QString>& entryIds) override;

private:

    enum class RecordingBound
    {
        start,
        end
    };

    std::unique_ptr<nx_http::HttpClient> initHttpClient() const;
    boost::optional<nx_http::BufferType> doRequest(const QString& requestPath, bool expectOnlyBody = false);

    boost::optional<uint64_t> getRecordingBound(RecordingBound bound);
    boost::optional<uint64_t> getRecordingStart();
    boost::optional<uint64_t> getRecordingEnd();

    std::vector<QString> getDirectoryList();
    bool fetchFileList(const QString& directory, std::vector<QString>* outFileLists);

    boost::optional<uint64_t> parseDate(
        const QString& dateTimeStr,
        const QString& dateTimeFormat);

    bool synchronizeArchive();

    std::vector<RemoteArchiveEntry> filterEntries(
        const std::vector<RemoteArchiveEntry>& allEntries);

    bool moveEntryToArchive(const RemoteArchiveEntry& entry);

    bool writeEntry(const RemoteArchiveEntry& entry, nx_http::BufferType* buffer);

    bool convertAndWriteBuffer(
        nx_http::BufferType* buffer,
        const QString& fileName,
        int64_t startTimeMs,
        const QnResourceAudioLayoutPtr& audioLayout,
        bool needMotion);

    QnServer::ChunksCatalog chunksCatalogByResolution(const QSize& resolution) const;

    bool isMotionDetectionNeeded(QnServer::ChunksCatalog catalog) const;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
