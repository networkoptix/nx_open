/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_authorization_manager_h
#define cloud_db_authorization_manager_h

#include <nx/utils/stree/resourcecontainer.h>

#include "../managers/managers_types.h"
#include "auth_types.h"


namespace nx {
namespace cdb {

class AccountManager;
class StreeManager;
class SystemManager;

//!Performs request authorization based on authentication info, requested entity and requested action type
/*!
    \note Requests from other classes of same module (same process) are authorized by \a AccessRole::cloudDB
    \note Probably, only access role has to be checked, not user credentials
 */
class AuthorizationManager
{
public:
    AuthorizationManager(
        const StreeManager& stree,
        const AccountManager& accountManager,
        const SystemManager& systemManager);

    /*!
        \param dataToAuthorize Authorization information can contain data filter to restrict access to data
        \return \a true if authorized, \a false otherwise
        \note This method cannot block
    */
    bool authorize(
        const nx::utils::stree::AbstractResourceReader& authenticationProperties,
        const nx::utils::stree::AbstractResourceReader& dataToAuthorize,
        EntityType requestedEntity,
        DataActionType requestedAction,
        nx::utils::stree::AbstractResourceWriter* const authzInfo ) const;

private:
    const StreeManager& m_stree;
    const AccountManager& m_accountManager;
    const SystemManager& m_systemManager;
};

}   //cdb
}   //nx

#endif
