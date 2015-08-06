/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_authorization_manager_h
#define cloud_db_authorization_manager_h

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/common/singleton.h>

#include "managers/types.h"
#include "types.h"


namespace cdb {

//!Performs request authorization based on authentication info, requested entity and requested action type
/*!
    \note Requests from other classes of same module (same process) are authorized by \a AccessRole::cloudDB
    \note Probably, only access role has to be checked, not user credentials
 */
class AuthorizationManager
:
    public Singleton<AuthorizationManager>
{
public:
    /*!
        \param dataToAuthorize Authorization information can contain data filter to restrict access to data
        \return \a true if authorized, \a false otherwise
        \note This method cannot block
    */
    bool authorize(
        const stree::AbstractResourceReader& dataToAuthorize,
        EntityType requestedEntity,
        DataActionType requestedAction,
        stree::AbstractResourceWriter* const authzInfo ) const;
};

}   //cdb

#endif
