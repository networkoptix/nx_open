#pragma once

#include <nx/sql/query_context.h>
#include <nx/sql/types.h>

#include <nx/cloud/db/api/system_data.h>

namespace nx::cloud::db {

enum class SharingType
{
    sharingWithExistingAccount,
    invite,
};

class AbstractSystemSharingExtension
{
public:
    virtual ~AbstractSystemSharingExtension() = default;

    /**
     * Called as the last step of adding NEW sharing (not updating existing one).
     */
    virtual nx::sql::DBResult afterSharingSystem(
        nx::sql::QueryContext* const queryContext,
        const api::SystemSharing& sharing,
        SharingType sharingType) = 0;
};

class AbstractSystemSharingManager
{
public:
    virtual ~AbstractSystemSharingManager() = default;

    /**
     * @return api::SystemAccessRole::none is returned if
     * - accountEmail has no rights for systemId
     * - accountEmail or systemId is unknown
     */
    virtual api::SystemAccessRole getAccountRightsForSystem(
        const std::string& accountEmail,
        const std::string& systemId) const = 0;

    virtual boost::optional<api::SystemSharingEx> getSystemSharingData(
        const std::string& accountEmail,
        const std::string& systemId) const = 0;

    virtual std::vector<api::SystemSharingEx> fetchSystemUsers(
        sql::QueryContext* queryContext,
        const std::string& systemId) = 0;

    virtual void addSystemSharingExtension(AbstractSystemSharingExtension* extension) = 0;
    virtual void removeSystemSharingExtension(AbstractSystemSharingExtension* extension) = 0;
};

} // namespace nx::cloud::db
