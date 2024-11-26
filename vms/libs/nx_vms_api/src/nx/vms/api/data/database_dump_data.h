// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>

#include <nx/reflect/instrument.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API DatabaseDumpData
{
    /**%apidoc Binary database dump, encoded in Base64. */
    QByteArray data;

    bool operator==(const DatabaseDumpData& other) const = default;
};
#define DatabaseDumpData_Fields (data)
NX_VMS_API_DECLARE_STRUCT(DatabaseDumpData)
NX_REFLECTION_INSTRUMENT(DatabaseDumpData, DatabaseDumpData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
