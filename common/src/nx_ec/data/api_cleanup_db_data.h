#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{

struct ApiCleanupDatabaseData: nx::vms::api::Data
{
    bool cleanupDbObjects = false;
    bool cleanupTransactionLog = false;
    QString reserved;
};

#define ApiCleanupDatabaseData_Fields (cleanupDbObjects)(cleanupTransactionLog)(reserved)

}
