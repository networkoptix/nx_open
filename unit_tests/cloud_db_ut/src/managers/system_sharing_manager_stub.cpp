#include "system_sharing_manager_stub.h"

namespace nx {
namespace cdb {
namespace test {

api::SystemAccessRole SystemSharingManagerStub::getAccountRightsForSystem(
    const std::string& /*accountEmail*/,
    const std::string& /*systemId*/) const
{
    return api::SystemAccessRole::none;
}

boost::optional<api::SystemSharingEx> SystemSharingManagerStub::getSystemSharingData(
    const std::string& /*accountEmail*/,
    const std::string& /*systemId*/) const
{
    return boost::none;
}

void SystemSharingManagerStub::addSystemSharingExtension(
    AbstractSystemSharingExtension* /*extension*/)
{
}

void SystemSharingManagerStub::removeSystemSharingExtension(
    AbstractSystemSharingExtension* /*extension*/)
{
}

} // namespace test
} // namespace cdb
} // namespace nx
