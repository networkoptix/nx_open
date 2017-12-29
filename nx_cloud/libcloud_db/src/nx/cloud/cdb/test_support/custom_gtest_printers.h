#pragma once

#include <iostream>

#include <nx/cloud/cdb/api/account_data.h>
#include <nx/cloud/cdb/api/result_code.h>
#include <nx/cloud/cdb/api/system_data.h>

#include "../managers/vms_gateway.h"

namespace nx {
namespace cdb {

void PrintTo(VmsResultCode val, ::std::ostream* os);

namespace api {

void PrintTo(AccountStatus val, ::std::ostream* os);
void PrintTo(ResultCode val, ::std::ostream* os);
void PrintTo(SystemStatus val, ::std::ostream* os);

} // namespace api
} // namespace cdb
} // namespace nx
