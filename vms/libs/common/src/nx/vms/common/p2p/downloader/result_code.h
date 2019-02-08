#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {

#ifdef THIS_BLOCK_IS_REQUIRED_TO_MAKE_FILE_BE_PROCESSED_BY_MOC_DO_NOT_DELETE
Q_OBJECT
#endif
QN_DECLARE_METAOBJECT_HEADER(downloader, ResultCode, )

enum class ResultCode
{
    ok,
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

QString toString(ResultCode code);

QN_FUSION_DECLARE_FUNCTIONS(downloader::ResultCode, (lexical))

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
