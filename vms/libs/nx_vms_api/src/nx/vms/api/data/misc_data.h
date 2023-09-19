// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
NX_VMS_API_DECLARE_STRUCT_AND_LIST(MiscData)

} // namespace api
} // namespace vms
} // namespace nx
