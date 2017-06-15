#pragma once

#include <utils/db/filter.h>
#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "../abstract_system_data_object.h"

namespace nx {
namespace cdb {
namespace dao {
namespace rdb {

class SystemDataObject:
    public AbstractSystemDataObject
{
public:
    SystemDataObject(const conf::Settings& settings);

    virtual nx::db::DBResult insert(
        nx::db::QueryContext* const queryContext,
        const data::SystemData& system,
        const std::string& accountId) override;

    virtual nx::db::DBResult selectSystemSequence(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        std::uint64_t* const sequence) override;

    virtual nx::db::DBResult markSystemAsDeleted(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual nx::db::DBResult deleteSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual nx::db::DBResult execSystemNameUpdate(
        nx::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) override;

    virtual nx::db::DBResult execSystemOpaqueUpdate(
        nx::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) override;

    virtual nx::db::DBResult activateSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual nx::db::DBResult fetchSystems(
        nx::db::QueryContext* queryContext,
        const nx::db::InnerJoinFilterFields& filterFields,
        std::vector<data::SystemData>* const systems) override;

    virtual nx::db::DBResult deleteExpiredSystems(
        nx::db::QueryContext* queryContext) override;

private:
    const conf::Settings& m_settings;
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
