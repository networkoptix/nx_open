#pragma once

#include <iostream>

#include <nx/cloud/cdb/api/result_code.h>

namespace nx {
namespace cdb {
namespace api {

void PrintTo(ResultCode val, ::std::ostream* os);

} // namespace api
} // namespace cdb
} // namespace nx
