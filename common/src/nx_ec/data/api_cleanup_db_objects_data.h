#pragma once

#include "api_globals.h"
#include "api_data.h"

namespace ec2
{

struct ApiCleanupDanglingDbObjectsData: ApiData
{
    bool cleanupDbObjects;
    bool cleanupTransactionLog;
    QString reserved;
};

#define ApiCleanupDanglingDbObjectsData_Fields (cleanupDbObjects)(cleanupTransactionLog)(reserved)

}
