#pragma once

#include <utils/db/filter.h>
#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "../../data/system_data.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class SystemHealthHistoryDataObject
{
public:
    SystemHealthHistoryDataObject();

    nx::db::DBResult insert(
        nx::db::QueryContext* queryContext,
        const std::string& systemId,
        const api::SystemHealthHistoryItem& historyItem);

    nx::db::DBResult selectHistoryBySystem(
        nx::db::QueryContext* queryContext,
        const std::string& systemId,
        api::SystemHealthHistory* history);
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
