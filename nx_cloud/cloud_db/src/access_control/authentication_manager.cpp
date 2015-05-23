/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#include "authentication_manager.h"


bool AuthenticationManager::authenticate(
    const stree::AbstractResourceReader& inputParams,
    std::function<void(AuthenticationInfo)> completionHandler ) const
{
    //TODO #ak authenticating by user name
    //TODO #ak authenticating by system name
    //TODO #ak authenticating with stree (to identify nx_cloud module)
    return false;
}
