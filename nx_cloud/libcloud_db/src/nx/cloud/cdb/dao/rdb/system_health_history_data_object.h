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

    nx::utils::db::DBResult insert(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId,
        const api::SystemHealthHistoryItem& historyItem);

    nx::utils::db::DBResult selectHistoryBySystem(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId,
        api::SystemHealthHistory* history);
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
