#pragma once

#include <nx/sql/filter.h>
#include <nx/sql/types.h>
#include <nx/sql/query_context.h>

#include "../data/system_data.h"

namespace nx::cloud::db {

namespace conf { class Settings; }

namespace dao {

class AbstractSystemDataObject
{
public:
    virtual ~AbstractSystemDataObject() = default;

    virtual nx::sql::DBResult insert(
        nx::sql::QueryContext* const queryContext,
        const data::SystemData& system,
        const std::string& accountId) = 0;

    virtual nx::sql::DBResult selectSystemSequence(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId,
        std::uint64_t* const sequence) = 0;

    virtual nx::sql::DBResult markSystemForDeletion(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual nx::sql::DBResult deleteSystem(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual nx::sql::DBResult execSystemNameUpdate(
        nx::sql::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) = 0;

    virtual nx::sql::DBResult execSystemOpaqueUpdate(
        nx::sql::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) = 0;

    virtual nx::sql::DBResult updateSystemStatus(
        nx::sql::QueryContext* const queryContext,
        const std::string& systemId,
        api::SystemStatus systemStatus) = 0;

    virtual nx::sql::DBResult fetchSystems(
        nx::sql::QueryContext* queryContext,
        const nx::sql::InnerJoinFilterFields& filterFields,
        std::vector<data::SystemData>* const systems) = 0;

    virtual boost::optional<data::SystemData> fetchSystemById(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId) = 0;

    virtual nx::sql::DBResult deleteExpiredSystems(
        nx::sql::QueryContext* queryContext) = 0;
};

class SystemDataObjectFactory
{
public:
    using CustomFactoryFunc =
        nx::utils::MoveOnlyFunc<
            std::unique_ptr<AbstractSystemDataObject>(const conf::Settings&)>;

    static std::unique_ptr<AbstractSystemDataObject> create(const conf::Settings&);
    /**
     * @return Factory func before call.
     */
    static CustomFactoryFunc setCustomFactoryFunc(CustomFactoryFunc func);
};

} // namespace dao
} // namespace nx::cloud::db
