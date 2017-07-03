#pragma once

#include <chrono>
#include <iostream>

#include <QByteArray>
#include <QString>

#include <nx/cloud/cdb/api/result_code.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx/utils/db/types.h>

namespace nx {
namespace cdb {
namespace api {

void PrintTo(ResultCode val, ::std::ostream* os);

} // namespace api
} // namespace cdb
} // namespace nx

namespace nx {
namespace utils {
namespace db {

void PrintTo(const DBResult val, ::std::ostream* os);

} // namespace db
} // namespace utils
} // namespace nx

namespace ec2 {

void PrintTo(ErrorCode val, ::std::ostream* os);

} // namespace ec2
