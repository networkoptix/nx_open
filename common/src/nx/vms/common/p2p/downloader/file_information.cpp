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

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(FileInformation, Status)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(FileInformation, PeerSelectionPolicy)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    FileInformation, (json)(eq), FileInformation_Fields, (optional, true)(brief, true))

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
