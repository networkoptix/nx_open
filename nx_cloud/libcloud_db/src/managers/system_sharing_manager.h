#pragma once

#include <utils/db/query_context.h>
#include <utils/db/types.h>

#include <cdb/system_data.h>

namespace nx {
namespace cdb {

enum class SharingType
{
    sharingWithExistingAccount,
    invite,
};

class AbstractSystemSharingExtension
{
public:
    virtual ~AbstractSystemSharingExtension() = default;

    virtual nx::db::DBResult afterSharingSystem(
        nx::db::QueryContext* const queryContext,
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

    virtual void addSystemSharingExtension(AbstractSystemSharingExtension* extension) = 0;
    virtual void removeSystemSharingExtension(AbstractSystemSharingExtension* extension) = 0;
};

} // namespace cdb
} // namespace nx
