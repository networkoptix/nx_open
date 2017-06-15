#pragma once

#include <deque>

#include <utils/db/filter.h>
#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "../../data/system_data.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class SystemSharingDataObject
{
public:
    nx::db::DBResult insertOrReplaceSharing(
        nx::db::QueryContext* const queryContext,
        const api::SystemSharingEx& sharing);

    nx::db::DBResult fetchAllUserSharings(
        nx::db::QueryContext* const queryContext,
        std::deque<api::SystemSharingEx>* const sharings);

    nx::db::DBResult fetchUserSharingsByAccountEmail(
        nx::db::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::deque<api::SystemSharingEx>* const sharings);

    nx::db::DBResult fetchSharing(
        nx::db::QueryContext* const queryContext,
        const std::string& accountEmail,
        const std::string& systemId,
        api::SystemSharingEx* const sharing);

    nx::db::DBResult fetchSharing(
        nx::db::QueryContext* const queryContext,
        const nx::db::InnerJoinFilterFields& filter,
        api::SystemSharingEx* const sharing);

    nx::db::DBResult deleteSharing(
        nx::db::QueryContext* queryContext,
        const std::string& systemId,
        const nx::db::InnerJoinFilterFields& filterFields = nx::db::InnerJoinFilterFields());

    /**
     * New system usage frequency is calculated as max(usage frequency of all account's systems) + 1.
     */
    nx::db::DBResult calculateUsageFrequencyForANewSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& accountId,
        const std::string& systemId,
        float* const newSystemInitialUsageFrequency);

    nx::db::DBResult updateUserLoginStatistics(
        nx::db::QueryContext* queryContext,
        const std::string& accountId,
        const std::string& systemId,
        std::chrono::system_clock::time_point lastloginTime,
        float usageFrequency);

private:
    nx::db::DBResult fetchUserSharings(
        nx::db::QueryContext* const queryContext,
        const nx::db::InnerJoinFilterFields& filterFields,
        std::deque<api::SystemSharingEx>* const sharings);
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
