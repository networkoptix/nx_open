#include "system_sharing_manager_stub.h"

namespace nx {
namespace cdb {
namespace test {

api::SystemAccessRole SystemSharingManagerStub::getAccountRightsForSystem(
    const std::string& accountEmail,
    const std::string& systemId) const
{
    auto sharing = getSystemSharingData(accountEmail, systemId);
    if (!sharing)
        return api::SystemAccessRole::none;
    return sharing->accessRole;
}

boost::optional<api::SystemSharingEx> SystemSharingManagerStub::getSystemSharingData(
    const std::string& accountEmail,
    const std::string& systemId) const
{
    QnMutexLocker lock(&m_mutex);

    auto sharingIter = m_sharings.find(std::make_pair(accountEmail, systemId));
    if (sharingIter == m_sharings.end())
        return boost::none;
    return sharingIter->second;
}

void SystemSharingManagerStub::addSystemSharingExtension(
    AbstractSystemSharingExtension* /*extension*/)
{
}

void SystemSharingManagerStub::removeSystemSharingExtension(
    AbstractSystemSharingExtension* /*extension*/)
{
}

void SystemSharingManagerStub::add(api::SystemSharingEx sharing)
{
    QnMutexLocker lock(&m_mutex);

    m_sharings.emplace(
        std::make_pair(sharing.accountEmail, sharing.systemId),
        sharing);
}

} // namespace test
} // namespace cdb
} // namespace nx
