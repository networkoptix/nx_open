#include "file_information.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

FileInformation::FileInformation(const QString& fileName):
    name(fileName)
{
}

bool FileInformation::isValid() const
{
    return !name.isEmpty();
}

bool operator==(const FileInformation& lhs, const FileInformation& rhs)
{
    return lhs.chunkSize == rhs.chunkSize && lhs.downloadedChunks == rhs.downloadedChunks
        && lhs.md5 == rhs.md5 && lhs.name == rhs.name && lhs.peerPolicy == rhs.peerPolicy
        && lhs.size == rhs.size && lhs.status == rhs.status && lhs.touchTime == rhs.touchTime
        && lhs.ttl == rhs.ttl && lhs.url == rhs.url;
}

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(FileInformation, Status)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(FileInformation, PeerSelectionPolicy)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(FileInformation, (json), FileInformation_Fields, (optional, true))

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
