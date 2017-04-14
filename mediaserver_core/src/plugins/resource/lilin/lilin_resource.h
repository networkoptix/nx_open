#pragma once

#include <plugins/resource/onvif/onvif_resource.h>
#include <plugins/common_interfaces/abstract_remote_archive_fetcher.h>


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
    virtual ~LilinResource();

    virtual CameraDiagnostics::Result initInternal() override;

    // Implementation of AbstractRemoteArchiveFetcher::listAvailableArchiveEntries 
    virtual bool listAvailableArchiveEntries(
        std::vector<RemoteArchiveEntry>* outArchiveEntries,
        uint64_t startTimeMs /* = 0 */,
        uint64_t endTimeMs /* = std::numeric_limits<uint64_t>::max() */) const override;

    // Implementation of AbstractRemoteArchiveFetcher::fetchArchiveEntries
    virtual bool fetchArchiveEntry(const QString& entryId, BufferType* outBuffer) override;

    // Implementation of AbstractRemoteArchiveFetcher::removeArchiveEntry
    virtual bool removeArchiveEntry(const QString& entryId) override;
};

} // namespace plugins
} // namespace mediaserver_core
} // namesapce nx