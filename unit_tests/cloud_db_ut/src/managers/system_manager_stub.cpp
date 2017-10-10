#include "system_manager_stub.h"

namespace nx {
namespace cdb {
namespace test {

boost::optional<api::SystemData> SystemManagerStub::findSystemById(const std::string& /*id*/) const
{
    // TODO
    return boost::none;
}

nx::utils::db::DBResult SystemManagerStub::updateSystemStatus(
    nx::utils::db::QueryContext* /*queryContext*/,
    const std::string& /*systemId*/,
    api::SystemStatus /*systemStatus*/)
{
    // TODO
    return nx::utils::db::DBResult::ok;
}

} // namespace test
} // namespace cdb
} // namespace nx
