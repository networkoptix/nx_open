#pragma once

#if defined (ENABLE_ONVIF)

#include <QtCore/QObject>
#include <QtCore/QString>

#include <vector>

#include <boost/optional/optional.hpp>

#include <core/resource/resource_fwd.h>
#include <core/resource/abstract_remote_archive_manager.h>
#include <nx/network/http/http_client.h>

namespace nx {
namespace mediaserver_core {
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

    std::unique_ptr<nx_http::HttpClient> initHttpClient() const;
    boost::optional<nx_http::BufferType> doRequest(const QString& requestPath, bool expectOnlyBody = false);

    std::vector<QString> getDirectoryList();
    bool fetchFileList(const QString& directory, std::vector<QString>* outFileLists);

private:
    LilinResource* m_resource;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // ENABLE ONVIF
