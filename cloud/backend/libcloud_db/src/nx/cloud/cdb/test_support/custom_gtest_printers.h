#pragma once

#include <iostream>

#include <nx/cloud/cdb/api/account_data.h>
#include <nx/cloud/cdb/api/result_code.h>
#include <nx/cloud/cdb/api/system_data.h>

#include "../managers/vms_request_result.h"

namespace nx::cloud::db {

void PrintTo(VmsResultCode val, ::std::ostream* os);

namespace api {

void PrintTo(AccountStatus val, ::std::ostream* os);
void PrintTo(ResultCode val, ::std::ostream* os);
void PrintTo(SystemStatus val, ::std::ostream* os);

} // namespace api
} // namespace nx::cloud::db
