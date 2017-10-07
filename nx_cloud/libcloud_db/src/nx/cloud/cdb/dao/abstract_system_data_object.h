#pragma once

#include <nx/utils/db/filter.h>
#include <nx/utils/db/types.h>
#include <nx/utils/db/query_context.h>

#include "../data/system_data.h"

namespace nx {
namespace cdb {

namespace conf { class Settings; }

namespace dao {

class AbstractSystemDataObject
{
public:
    virtual ~AbstractSystemDataObject() = default;

    virtual nx::utils::db::DBResult insert(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemData& system,
        const std::string& accountId) = 0;

    virtual nx::utils::db::DBResult selectSystemSequence(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        std::uint64_t* const sequence) = 0;

    virtual nx::utils::db::DBResult markSystemAsDeleted(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual nx::utils::db::DBResult deleteSystem(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual nx::utils::db::DBResult execSystemNameUpdate(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) = 0;

    virtual nx::utils::db::DBResult execSystemOpaqueUpdate(
        nx::utils::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) = 0;

    virtual nx::utils::db::DBResult updateSystemStatus(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId,
        api::SystemStatus systemStatus) = 0;

    virtual nx::utils::db::DBResult fetchSystems(
        nx::utils::db::QueryContext* queryContext,
        const nx::utils::db::InnerJoinFilterFields& filterFields,
        std::vector<data::SystemData>* const systems) = 0;

    virtual boost::optional<data::SystemData> fetchSystemById(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId) = 0;

    virtual nx::utils::db::DBResult deleteExpiredSystems(
        nx::utils::db::QueryContext* queryContext) = 0;
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
} // namespace cdb
} // namespace nx
