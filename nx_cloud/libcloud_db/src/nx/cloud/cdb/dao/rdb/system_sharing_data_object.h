#pragma once

#include <deque>

#include <nx/utils/db/filter.h>
#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

#include "../../data/system_data.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class SystemSharingDataObject
{
public:
    nx::utils::db::DBResult insertOrReplaceSharing(
        nx::utils::db::QueryContext* const queryContext,
        const api::SystemSharingEx& sharing);

    nx::utils::db::DBResult fetchAllUserSharings(
        nx::utils::db::QueryContext* const queryContext,
        std::deque<api::SystemSharingEx>* const sharings);

    nx::utils::db::DBResult fetchUserSharingsByAccountEmail(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::deque<api::SystemSharingEx>* const sharings);

    nx::utils::db::DBResult fetchSharing(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& accountEmail,
        const std::string& systemId,
        api::SystemSharingEx* const sharing);

    nx::utils::db::DBResult fetchSharing(
        nx::utils::db::QueryContext* const queryContext,
        const nx::utils::db::InnerJoinFilterFields& filter,
        api::SystemSharingEx* const sharing);

    nx::utils::db::DBResult deleteSharing(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId,
        const nx::utils::db::InnerJoinFilterFields& filterFields = nx::utils::db::InnerJoinFilterFields());

    /**
     * New system usage frequency is calculated as max(usage frequency of all account's systems) + 1.
     */
    nx::utils::db::DBResult calculateUsageFrequencyForANewSystem(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& accountId,
        const std::string& systemId,
        float* const newSystemInitialUsageFrequency);

    nx::utils::db::DBResult updateUserLoginStatistics(
        nx::utils::db::QueryContext* queryContext,
        const std::string& accountId,
        const std::string& systemId,
        std::chrono::system_clock::time_point lastloginTime,
        float usageFrequency);

private:
    nx::utils::db::DBResult fetchUserSharings(
        nx::utils::db::QueryContext* const queryContext,
        const nx::utils::db::InnerJoinFilterFields& filterFields,
        std::deque<api::SystemSharingEx>* const sharings);
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
