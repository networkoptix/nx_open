#pragma once

#if defined (ENABLE_ONVIF)

#include <QtCore/QObject>
#include <QtCore/QString>

#include <vector>

#include <boost/optional/optional.hpp>

#include <core/resource/abstract_remote_archive_manager.h>
#include <nx/network/http/http_client.h>
#include <nx/vms/server/resource/resource_fwd.h>

namespace nx {
namespace vms::server {
namespace plugins {

class LilinRemoteArchiveManager:
    public QObject,
    public nx::core::resource::AbstractRemoteArchiveManager
{
    Q_OBJECT;

    using RemoteArchiveEntry = nx::core::resource::RemoteArchiveChunk;
    using BufferType = nx::core::resource::BufferType;

public:

    explicit LilinRemoteArchiveManager(LilinResource* resource);
    virtual ~LilinRemoteArchiveManager() = default;

    virtual bool listAvailableArchiveEntries(
        nx::core::resource::OverlappedRemoteChunks* outArchiveEntries,
        int64_t startTimeMs = 0,
        int64_t endTimeMs = std::numeric_limits<int64_t>::max()) override;

    virtual nx::core::resource::ImportOrder overlappedIdImportOrder() const override;

    virtual std::unique_ptr<QnAbstractArchiveDelegate> archiveDelegate(
        const nx::core::resource::RemoteArchiveChunk& chunk) override;

    virtual bool fetchArchiveEntry(const QString& entryId, BufferType* outBuffer) override;

    virtual bool removeArchiveEntries(const std::vector<QString>& entryIds) override;

    virtual nx::core::resource::RemoteArchiveCapabilities capabilities() const override;

    virtual nx::core::resource::RemoteArchiveSynchronizationSettings settings() const override;

    virtual void beforeSynchronization() override;

    virtual void afterSynchronization(bool isSynchronizationSuccessful) override;

private:

    enum class RecordingBound
    {
        start,
        end
    };

    std::unique_ptr<nx::network::http::HttpClient> initHttpClient() const;
    boost::optional<nx::network::http::BufferType> doRequest(const QString& requestPath, bool expectOnlyBody = false);

    boost::optional<int64_t> getRecordingBound(RecordingBound bound);
    boost::optional<int64_t> getRecordingStart();
    boost::optional<int64_t> getRecordingEnd();

    std::vector<QString> getDirectoryList();
    bool fetchFileList(const QString& directory, std::vector<QString>* outFileLists);

    boost::optional<int64_t> parseDate(
        const QString& dateTimeStr,
        const QString& dateTimeFormat);

private:
    LilinResource* m_resource;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx

#endif // ENABLE ONVIF
