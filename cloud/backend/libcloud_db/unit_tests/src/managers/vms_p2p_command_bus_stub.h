#pragma once

#include <nx/utils/move_only_func.h>

#include <nx/cloud/db/ec2/vms_p2p_command_bus.h>

namespace nx::cloud::db {
namespace test {

using OnSaveResourceAttribute =
    nx::utils::MoveOnlyFunc<nx::sql::DBResult(
        const std::string& /*systemId*/,
        nx::vms::api::ResourceParamWithRefData)>;

class VmsP2pCommandBusStub:
    public ec2::AbstractVmsP2pCommandBus
{
public:
    virtual nx::sql::DBResult saveResourceAttribute(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        nx::vms::api::ResourceParamWithRefData data) override;

    void setOnSaveResourceAttribute(OnSaveResourceAttribute func);

private:
    OnSaveResourceAttribute m_onSaveResourceAttribute;
};

} // namespace test
} // namespace nx::cloud::db
