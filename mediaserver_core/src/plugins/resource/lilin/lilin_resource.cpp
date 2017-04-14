#include "lilin_resource.h"

namespace nx {
namespace mediaserver_core {
namespace plugins {

LilinResource::LilinResource():
    QnPlOnvifResource()
{

}

LilinResource::~LilinResource()
{

}

CameraDiagnostics::Result LilinResource::initInternal()
{
    return base_type::initInternal();
}

bool LilinResource::listAvailableArchiveEntries(std::vector<RemoteArchiveEntry>* outArchiveEntries, uint64_t startTimeMs /* = 0 */, uint64_t endTimeMs /* = std::numeric_limits<uint64_t>::max() */) const
{
    return false;
}

bool LilinResource::fetchArchiveEntry(const QString& entryId, BufferType* outBuffer)
{
    return false;
}

bool LilinResource::removeArchiveEntry(const QString& entryId)
{
    return false;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx