#pragma once

#include "key_value_data.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API MiscData: KeyValueData
{
    using KeyValueData::KeyValueData; //< Forward constructors.
};

#define MiscData_Fields \
    KeyValueData_Fields

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::MiscData)
Q_DECLARE_METATYPE(nx::vms::api::MiscDataList)
