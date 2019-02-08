#pragma once

#include "data.h"

#include <QtCore/QString>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API CleanupDatabaseData: Data
{
    bool cleanupDbObjects = false;
    bool cleanupTransactionLog = false;
    QString reserved;
};

#define CleanupDatabaseData_Fields \
    (cleanupDbObjects) \
    (cleanupTransactionLog) \
    (reserved)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::CleanupDatabaseData)
