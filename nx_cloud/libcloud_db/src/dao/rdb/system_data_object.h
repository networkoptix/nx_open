#pragma once

#include <utils/db/filter.h>
#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "data/system_data.h"

namespace nx {
namespace cdb {
namespace conf {

class Settings;

} // namespace conf

namespace dao {
namespace rdb {

class SystemDataObject
{
public:
    SystemDataObject(const conf::Settings& settings);

    nx::db::DBResult insert(
        nx::db::QueryContext* const queryContext,
        const data::SystemData& system,
        const std::string& accountId);

    nx::db::DBResult selectSystemSequence(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        std::uint64_t* const sequence);

    nx::db::DBResult markSystemAsDeleted(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId);

    nx::db::DBResult deleteSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId);

    nx::db::DBResult execSystemNameUpdate(
        nx::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data);

    nx::db::DBResult execSystemOpaqueUpdate(
        nx::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data);

    nx::db::DBResult activateSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId);

    nx::db::DBResult fetchSystems(
        nx::db::QueryContext* queryContext,
        const nx::db::InnerJoinFilterFields& filterFields,
        std::vector<data::SystemData>* const systems);

    nx::db::DBResult deleteExpiredSystems(nx::db::QueryContext* queryContext);

private:
    const conf::Settings& m_settings;
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
