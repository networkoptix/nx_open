#pragma once

#include <nx/sql/filter.h>
#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../../data/system_data.h"

namespace nx::cloud::db {
namespace dao {
namespace rdb {

class SystemHealthHistoryDataObject
{
public:
    SystemHealthHistoryDataObject();

    nx::sql::DBResult insert(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        const api::SystemHealthHistoryItem& historyItem);

    nx::sql::DBResult selectHistoryBySystem(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        api::SystemHealthHistory* history);
};

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
