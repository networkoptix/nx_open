#pragma once

#include <vector>

#include <nx/sql/filter.h>
#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../../data/system_data.h"

namespace nx::cloud::db {
namespace dao {
namespace rdb {

class SystemSharingDataObject
{
public:
    nx::sql::DBResult insertOrReplaceSharing(
        nx::sql::QueryContext* const queryContext,
        const api::SystemSharingEx& sharing);

    nx::sql::DBResult fetchAllUserSharings(
        nx::sql::QueryContext* const queryContext,
        std::vector<api::SystemSharingEx>* const sharings);

    nx::sql::DBResult fetchUserSharingsByAccountEmail(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountEmail,
        std::vector<api::SystemSharingEx>* const sharings);

    nx::sql::DBResult fetchSharing(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountEmail,
        const std::string& systemId,
        api::SystemSharingEx* const sharing);

    nx::sql::DBResult fetchSharing(
        nx::sql::QueryContext* const queryContext,
        const nx::sql::InnerJoinFilterFields& filter,
        api::SystemSharingEx* const sharing);

    /**
     * NOTE: Throws on failure.
     */
    std::vector<api::SystemSharingEx> fetchSystemSharings(
        sql::QueryContext* queryContext,
        const std::string& systemId);

    nx::sql::DBResult deleteSharing(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        const nx::sql::InnerJoinFilterFields& filterFields = nx::sql::InnerJoinFilterFields());

    /**
     * New system usage frequency is calculated as max(usage frequency of all account's systems) + 1.
     */
    nx::sql::DBResult calculateUsageFrequencyForANewSystem(
        nx::sql::QueryContext* const queryContext,
        const std::string& accountId,
        const std::string& systemId,
        float* const newSystemInitialUsageFrequency);

    nx::sql::DBResult updateUserLoginStatistics(
        nx::sql::QueryContext* queryContext,
        const std::string& accountId,
        const std::string& systemId,
        std::chrono::system_clock::time_point lastloginTime,
        float usageFrequency);

private:
    nx::sql::DBResult fetchUserSharings(
        nx::sql::QueryContext* const queryContext,
        const nx::sql::InnerJoinFilterFields& filterFields,
        std::vector<api::SystemSharingEx>* const sharings);
};

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
