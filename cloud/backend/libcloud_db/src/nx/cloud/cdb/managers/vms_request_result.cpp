#include "vms_request_result.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::db {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::cloud::db, VmsResultCode,
    (nx::cloud::db::VmsResultCode::ok, "ok")
    (nx::cloud::db::VmsResultCode::invalidData, "invalidData")
    (nx::cloud::db::VmsResultCode::networkError, "networkError")
    (nx::cloud::db::VmsResultCode::forbidden, "forbidden")
    (nx::cloud::db::VmsResultCode::logicalError, "logicalError")
    (nx::cloud::db::VmsResultCode::unreachable, "unreachable")
)

} // namespace nx::cloud::db
