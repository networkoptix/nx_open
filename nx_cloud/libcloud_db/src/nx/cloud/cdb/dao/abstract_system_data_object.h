#pragma once

#include <utils/db/filter.h>
#include <utils/db/types.h>
#include <utils/db/query_context.h>

#include "../data/system_data.h"

namespace nx {
namespace cdb {

namespace conf { class Settings; }

namespace dao {

class AbstractSystemDataObject
{
public:
    virtual ~AbstractSystemDataObject() = default;

    virtual nx::db::DBResult insert(
        nx::db::QueryContext* const queryContext,
        const data::SystemData& system,
        const std::string& accountId) = 0;

    virtual nx::db::DBResult selectSystemSequence(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId,
        std::uint64_t* const sequence) = 0;

    virtual nx::db::DBResult markSystemAsDeleted(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual nx::db::DBResult deleteSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual nx::db::DBResult execSystemNameUpdate(
        nx::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) = 0;

    virtual nx::db::DBResult execSystemOpaqueUpdate(
        nx::db::QueryContext* const queryContext,
        const data::SystemAttributesUpdate& data) = 0;

    virtual nx::db::DBResult activateSystem(
        nx::db::QueryContext* const queryContext,
        const std::string& systemId) = 0;

    virtual nx::db::DBResult fetchSystems(
        nx::db::QueryContext* queryContext,
        const nx::db::InnerJoinFilterFields& filterFields,
        std::vector<data::SystemData>* const systems) = 0;

    virtual nx::db::DBResult deleteExpiredSystems(nx::db::QueryContext* queryContext) = 0;
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
