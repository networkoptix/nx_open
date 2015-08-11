/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#include "authorization_manager.h"


namespace nx {
namespace cdb {


bool AuthorizationManager::authorize(
    const stree::AbstractResourceReader& dataToAuthorize,
    EntityType requestedEntity,
    DataActionType requestedAction,
    stree::AbstractResourceWriter* const authzInfo ) const
{
    //TODO #ak
    return true;
}


}   //cdb
}   //nx
