#include "system_manager_stub.h"

namespace nx {
namespace cdb {
namespace test {

boost::optional<api::SystemData> SystemManagerStub::findSystemById(const std::string& id) const
{
    const auto it = m_systems.find(id);
    if (it == m_systems.end())
        return boost::none;
    return it->second;
}

nx::utils::db::DBResult SystemManagerStub::fetchSystemById(
    nx::utils::db::QueryContext* /*queryContext*/,
    const std::string& systemId,
    data::SystemData* const system)
{
    auto x = findSystemById(systemId);
    if (!x)
        return nx::utils::db::DBResult::notFound;

    system->api::SystemData::operator=(*x);
    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemManagerStub::updateSystemStatus(
    nx::utils::db::QueryContext* /*queryContext*/,
    const std::string& systemId,
    api::SystemStatus systemStatus)
{
    const auto it = m_systems.find(systemId);
    if (it == m_systems.end())
        return nx::utils::db::DBResult::notFound;

    it->second.status = systemStatus;
    return nx::utils::db::DBResult::ok;
}

nx::utils::db::DBResult SystemManagerStub::markSystemForDeletion(
    nx::utils::db::QueryContext* const queryContext,
    const std::string& systemId)
{
    return updateSystemStatus(queryContext, systemId, api::SystemStatus::deleted_);
}

void SystemManagerStub::addSystem(const api::SystemData& system)
{
    m_systems.emplace(system.id, system);
}

} // namespace test
} // namespace cdb
} // namespace nx
