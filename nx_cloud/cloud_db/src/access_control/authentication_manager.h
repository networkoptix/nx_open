/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_authentication_manager_h
#define cloud_db_authentication_manager_h

#include <utils/common/singleton.h>

#include "types.h"


//!Performs authentication based on various parameters
/*!
    Typically, username/digest is used to authenticate, but IP address/network interface and other data can be used also.
    Uses account data and some predefined static data to authenticate incoming requests.
    \note Listens to user data change events
 */
class AuthenticationManager
:
    public Singleton<AuthenticationManager>
{
public:
    /*!
        \param inputParams It can be HTTP request and originating peer network info
        \note \a completionHandler is allowed to be called from within this method
    */
    bool authenticate(
        const stree::AbstractResourceReader& inputParams,
        std::function<void(AuthenticationInfo)> completionHandler ) const;
};

#endif
