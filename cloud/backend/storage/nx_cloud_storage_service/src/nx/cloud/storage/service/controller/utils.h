#pragma once

#include <nx/sql/types.h>
#include <nx/network/cloud/storage/service/api/result.h>

namespace nx::cloud::storage::service::controller::utils {

api::ResultCode toResultCode(nx::sql::DBResult dbResult);

} // namespace nx::cloud::storage::service::controller::utils