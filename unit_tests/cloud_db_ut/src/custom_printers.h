#pragma once

#include <chrono>
#include <iostream>

#include <QByteArray>
#include <QString>

#include <nx/cloud/cdb/api/result_code.h>
#include <nx/cloud/cdb/managers/vms_gateway.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx/utils/db/types.h>

namespace nx {
namespace cdb {

void PrintTo(VmsResultCode val, ::std::ostream* os);

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
