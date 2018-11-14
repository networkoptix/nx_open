#include "vms_request_result.h"

#include <nx/fusion/model_functions.h>

namespace nx::cdb {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cdb, VmsResultCode,
    (nx::cdb::VmsResultCode::ok, "ok")
    (nx::cdb::VmsResultCode::invalidData, "invalidData")
    (nx::cdb::VmsResultCode::networkError, "networkError")
    (nx::cdb::VmsResultCode::forbidden, "forbidden")
    (nx::cdb::VmsResultCode::logicalError, "logicalError")
    (nx::cdb::VmsResultCode::unreachable, "unreachable")
)

} // namespace nx::cdb
