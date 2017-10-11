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

nx::utils::db::DBResult SystemManagerStub::updateSystemStatus(
    nx::utils::db::QueryContext* /*queryContext*/,
    const std::string& /*systemId*/,
    api::SystemStatus /*systemStatus*/)
{
    // TODO
    return nx::utils::db::DBResult::ok;
}

void SystemManagerStub::addSystem(const api::SystemData& system)
{
    m_systems.emplace(system.id, system);
}

} // namespace test
} // namespace cdb
} // namespace nx
