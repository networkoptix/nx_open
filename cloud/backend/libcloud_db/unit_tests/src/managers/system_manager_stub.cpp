#include "system_manager_stub.h"

namespace nx::cloud::db {
namespace test {

boost::optional<api::SystemData> SystemManagerStub::findSystemById(const std::string& id) const
{
    const auto it = m_systems.find(id);
    if (it == m_systems.end())
        return boost::none;
    return it->second;
}

nx::sql::DBResult SystemManagerStub::fetchSystemById(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& systemId,
    data::SystemData* const system)
{
    auto x = findSystemById(systemId);
    if (!x)
        return nx::sql::DBResult::notFound;

    system->api::SystemData::operator=(*x);
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManagerStub::updateSystemStatus(
    nx::sql::QueryContext* /*queryContext*/,
    const std::string& systemId,
    api::SystemStatus systemStatus)
{
    const auto it = m_systems.find(systemId);
    if (it == m_systems.end())
        return nx::sql::DBResult::notFound;

    it->second.status = systemStatus;
    return nx::sql::DBResult::ok;
}

nx::sql::DBResult SystemManagerStub::markSystemForDeletion(
    nx::sql::QueryContext* const queryContext,
    const std::string& systemId)
{
    return updateSystemStatus(queryContext, systemId, api::SystemStatus::deleted_);
}

void SystemManagerStub::addExtension(AbstractSystemExtension*)
{
}

void SystemManagerStub::removeExtension(AbstractSystemExtension*)
{
}

void SystemManagerStub::addSystem(const api::SystemData& system)
{
    m_systems.emplace(system.id, system);
}

} // namespace test
} // namespace nx::cloud::db
