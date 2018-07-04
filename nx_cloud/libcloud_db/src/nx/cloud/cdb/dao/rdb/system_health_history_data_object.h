#pragma once

#include <nx/utils/db/filter.h>
#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

#include "../../data/system_data.h"

namespace nx {
namespace cdb {
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
} // namespace cdb
} // namespace nx
