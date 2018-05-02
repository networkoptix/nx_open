#include "vms_p2p_command_bus_stub.h"

namespace nx {
namespace cdb {
namespace test {

nx::utils::db::DBResult VmsP2pCommandBusStub::saveResourceAttribute(
    nx::utils::db::QueryContext* /*queryContext*/,
    const std::string& systemId,
    nx::vms::api::ResourceParamWithRefData data)
{
    if (m_onSaveResourceAttribute)
        return m_onSaveResourceAttribute(systemId, data);

    return nx::utils::db::DBResult::ok;
}

void VmsP2pCommandBusStub::setOnSaveResourceAttribute(OnSaveResourceAttribute func)
{
    m_onSaveResourceAttribute = std::move(func);
}

} // namespace test
} // namespace cdb
} // namespace nx
