#pragma once

#include <deque>

#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

#include "../connection_statistics_info.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {

class AbstractDataObject
{
public:
    virtual ~AbstractDataObject() = default;

    virtual nx::utils::db::DBResult save(
        nx::utils::db::QueryContext* /*queryContext*/,
        ConnectSession connectionRecord) = 0;

    virtual nx::utils::db::DBResult readAllRecords(
        nx::utils::db::QueryContext* /*queryContext*/,
        std::deque<ConnectSession>* connectionRecords) = 0;
};

} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
