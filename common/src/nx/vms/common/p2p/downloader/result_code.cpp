#include "result_code.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(downloader, ResultCode)

QString toString(ResultCode code)
{
    return QnLexical::serialized(code);
}

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
