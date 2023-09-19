// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API DatabaseDumpToFileData
{
    /**%apidoc Binary database dump file size, in bytes. */
    qint64 size = 0;
};
#define DatabaseDumpToFileData_Fields (size)
NX_VMS_API_DECLARE_STRUCT(DatabaseDumpToFileData)

} // namespace api
} // namespace vms
} // namespace nx
