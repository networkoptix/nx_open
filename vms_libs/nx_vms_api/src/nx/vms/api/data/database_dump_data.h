#pragma once

#include "data.h"

namespace nx {
namespace vms {
namespace api {

struct DatabaseDumpData: Data
{
    QByteArray data;
};
#define DatabaseDumpData_Fields (data)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::DatabaseDumpData);
