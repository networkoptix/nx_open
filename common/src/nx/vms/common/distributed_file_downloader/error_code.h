#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace common {

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(distributed_file_downloader, ErrorCode, )

enum class ErrorCode
{
    noError,
    ioError,
    fileDoesNotExist,
    fileAlreadyExists,
    fileAlreadyDownloaded,
    invalidChecksum,
    invalidFileSize,
    invalidChunkIndex,
    invalidChunkSize,
    noFreeSpace
};

QN_FUSION_DECLARE_FUNCTIONS(distributed_file_downloader::ErrorCode, (lexical))

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
