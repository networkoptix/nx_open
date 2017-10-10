#pragma once

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

    virtual nx::utils::db::DBResult updateSystemStatus(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId,
        api::SystemStatus systemStatus) override;
};

} // namespace test
} // namespace cdb
} // namespace nx
