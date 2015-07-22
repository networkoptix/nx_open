/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_access_role_h
#define cloud_db_access_role_h

#include <plugins/videodecoder/stree/resourcecontainer.h>


enum class AccessRole
{
    //!anyone
    none,
    //!System, binded to account can fetch some data too
    system,
    user,
    //!Other nx_cloud module
    nxCloudModule,
    //!Used by classes of this module to communicate to each other. Everything is allowed for this role
    cloudDB
};

//!Contains information about authenticated user
/*!
 \note This information plus requested action type is enough to perform authorization
 */
class AuthenticationInfo
{
public:
    AccessRole role;
    //!Parameters, discovered during authentication (e.g., account id)
    stree::ResourceContainer params;
};

class AuthorizationInfo
{
public:
    bool accessAllowedToOwnDataOnly;
};

#endif
