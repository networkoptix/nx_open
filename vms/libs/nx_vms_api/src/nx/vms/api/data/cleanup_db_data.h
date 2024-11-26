// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/reflect/instrument.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API CleanupDatabaseData
{
    bool cleanupDbObjects = false;
    bool cleanupTransactionLog = false;
    QString reserved;
};
#define CleanupDatabaseData_Fields (cleanupDbObjects)(cleanupTransactionLog)(reserved)
NX_VMS_API_DECLARE_STRUCT(CleanupDatabaseData)
NX_REFLECTION_INSTRUMENT(CleanupDatabaseData, CleanupDatabaseData_Fields)

} // namespace api
} // namespace vms
} // namespace nx
