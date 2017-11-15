#pragma once

#include <nx/utils/move_only_func.h>

#include <nx/cloud/cdb/ec2/vms_p2p_command_bus.h>

namespace nx {
namespace cdb {
namespace test {

using OnSaveResourceAttribute =
    nx::utils::MoveOnlyFunc<nx::utils::db::DBResult(
        const std::string& /*systemId*/,
        ::ec2::ApiResourceParamWithRefData)>;

class VmsP2pCommandBusStub:
    public ec2::AbstractVmsP2pCommandBus
{
public:
    virtual nx::utils::db::DBResult saveResourceAttribute(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId,
        ::ec2::ApiResourceParamWithRefData data) override;

    void setOnSaveResourceAttribute(OnSaveResourceAttribute func);

private:
    OnSaveResourceAttribute m_onSaveResourceAttribute;
};

} // namespace test
} // namespace cdb
} // namespace nx
