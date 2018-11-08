#include <nx/fusion/model_functions.h>
#include "validate_result.h"

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

ValidateResult::ValidateResult(): success(false) {}
ValidateResult::ValidateResult(bool success): success(success) {}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ValidateResult, (json)(eq), ValidateResult_Fields, (optional, true)(brief, true))

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
