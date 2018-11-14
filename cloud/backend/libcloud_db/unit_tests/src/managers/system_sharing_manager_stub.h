#pragma once

#include <nx/cloud/db/managers/system_sharing_manager.h>
#include <nx/utils/thread/mutex.h>

namespace nx::cloud::db {
namespace test {

class SystemSharingManagerStub:
    public AbstractSystemSharingManager
{
public:
    virtual api::SystemAccessRole getAccountRightsForSystem(
        const std::string& accountEmail,
        const std::string& systemId) const override;

    virtual boost::optional<api::SystemSharingEx> getSystemSharingData(
        const std::string& accountEmail,
        const std::string& systemId) const override;

    virtual std::vector<api::SystemSharingEx> fetchSystemUsers(
        sql::QueryContext* queryContext,
        const std::string& systemId) override;

    virtual void addSystemSharingExtension(AbstractSystemSharingExtension* extension) override;
    virtual void removeSystemSharingExtension(AbstractSystemSharingExtension* extension) override;

    void add(api::SystemSharingEx sharing);

private:
    mutable QnMutex m_mutex;
    // map<pair<accountEmail, systemId>, sharing>
    std::map<std::pair<std::string, std::string>, api::SystemSharingEx> m_sharings;
};

} // namespace test
} // namespace nx::cloud::db
