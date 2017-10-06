#pragma once

#include <nx/utils/db/filter.h>
#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

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

    virtual nx::utils::db::DBResult insert(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemData& system,
        const std::string& accountId) override;

    virtual nx::utils::db::DBResult selectSystemSequence(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        std::uint64_t* const sequence) override;

    virtual nx::utils::db::DBResult markSystemAsDeleted(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual nx::utils::db::DBResult deleteSystem(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual nx::utils::db::DBResult execSystemNameUpdate(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) override;

    virtual nx::utils::db::DBResult execSystemOpaqueUpdate(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) override;

    virtual nx::utils::db::DBResult activateSystem(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual nx::utils::db::DBResult fetchSystems(
        nx::utils::db::QueryContext* queryContext,
        const nx::utils::db::InnerJoinFilterFields& filterFields,
        std::vector<data::SystemData>* const systems) override;

    virtual boost::optional<data::SystemData> fetchSystemById(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId) override;

    virtual nx::utils::db::DBResult deleteExpiredSystems(
        nx::utils::db::QueryContext* queryContext) override;

private:
    const conf::Settings& m_settings;
};

} // namespace rdb
} // namespace dao
} // namespace cdb
} // namespace nx
