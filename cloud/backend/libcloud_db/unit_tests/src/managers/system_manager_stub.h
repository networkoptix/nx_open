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

    virtual nx::sql::DBResult fetchSystemById(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        data::SystemData* const system) override;

    virtual nx::sql::DBResult updateSystemStatus(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        api::SystemStatus systemStatus) override;

    virtual nx::sql::DBResult markSystemForDeletion(
        nx::sql::QueryContext* const queryContext,
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
