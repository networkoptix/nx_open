#pragma once

#include "data.h"

namespace nx {
namespace vms {
namespace api {

struct DatabaseDumpToFileData: Data
{
    qint64 size = 0;
};
#define DatabaseDumpToFileData_Fields (size)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::DatabaseDumpToFileData);
