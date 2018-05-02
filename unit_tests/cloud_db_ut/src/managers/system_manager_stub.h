#pragma once

#include <map>

#include <nx/cloud/cdb/managers/system_manager.h>

namespace nx {
namespace cdb {
namespace test {

class SystemManagerStub:
    public AbstractSystemManager
{
public:
    virtual ~SystemManagerStub() = default;

    virtual boost::optional<api::SystemData> findSystemById(const std::string& id) const override;

    virtual nx::utils::db::DBResult fetchSystemById(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId,
        data::SystemData* const system) override;

    virtual nx::utils::db::DBResult updateSystemStatus(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId,
        api::SystemStatus systemStatus) override;

    virtual nx::utils::db::DBResult markSystemForDeletion(
        nx::utils::db::QueryContext* const queryContext,
        const std::string& systemId) override;

    virtual void addExtension(AbstractSystemExtension*) override;
    virtual void removeExtension(AbstractSystemExtension*) override;

    void addSystem(const api::SystemData& system);

private:
    std::map<std::string, api::SystemData> m_systems;
};

} // namespace test
} // namespace cdb
} // namespace nx
