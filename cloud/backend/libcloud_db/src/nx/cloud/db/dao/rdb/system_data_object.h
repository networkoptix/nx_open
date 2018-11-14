#pragma once

#include <nx/sql/filter.h>
#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../abstract_system_data_object.h"

namespace nx::cloud::db {
namespace dao {
namespace rdb {

class SystemDataObject:
    public AbstractSystemDataObject
{
public:
    SystemDataObject(const conf::Settings& settings);

    virtual nx::sql::DBResult insert(
        nx::sql::QueryContext* const queryContext,
        const data::SystemData& system,
        const std::string& accountId) override;

    virtual nx::sql::DBResult selectSystemSequence(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId,
        std::uint64_t* const sequence) override;

    virtual nx::sql::DBResult markSystemForDeletion(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual nx::sql::DBResult deleteSystem(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual nx::sql::DBResult execSystemNameUpdate(
        nx::sql::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) override;

    virtual nx::sql::DBResult execSystemOpaqueUpdate(
        nx::sql::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) override;

    virtual nx::sql::DBResult updateSystemStatus(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId,
        api::SystemStatus systemStatus) override;

    virtual nx::sql::DBResult fetchSystems(
        nx::sql::QueryContext* queryContext,
        const nx::sql::InnerJoinFilterFields& filterFields,
        std::vector<data::SystemData>* const systems) override;

    virtual boost::optional<data::SystemData> fetchSystemById(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId) override;

    virtual nx::sql::DBResult deleteExpiredSystems(
        nx::sql::QueryContext* queryContext) override;

private:
    const conf::Settings& m_settings;
};

} // namespace rdb
} // namespace dao
} // namespace nx::cloud::db
