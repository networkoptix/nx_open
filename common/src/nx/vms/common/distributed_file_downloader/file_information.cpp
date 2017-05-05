#include "file_information.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

FileInformation::FileInformation()
{
}

FileInformation::FileInformation(const QString& fileName):
    name(fileName)
{
}

bool FileInformation::isValid() const
{
    return !name.isEmpty();
}

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(FileInformation, Status)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(FileInformation, (json), FileInformation_Fields)

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
