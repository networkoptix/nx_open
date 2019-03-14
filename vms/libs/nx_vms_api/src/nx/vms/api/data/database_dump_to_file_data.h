#pragma once

#include "data.h"

#include <QtCore/QtGlobal>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API DatabaseDumpToFileData: Data
{
    qint64 size = 0;
};
#define DatabaseDumpToFileData_Fields (size)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::DatabaseDumpToFileData);
