/**********************************************************
* 3 may 2015
* a.kolesnikov
***********************************************************/

#ifndef CLOUD_DB_AUTH_TYPES_H
#define CLOUD_DB_AUTH_TYPES_H

#include <plugins/videodecoder/stree/resourcecontainer.h>


namespace nx {
namespace cdb {

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
:
    public stree::AbstractResourceReader
{
public:
    AuthorizationInfo();
    AuthorizationInfo( stree::ResourceContainer&& rc );

    //!Implementation of \a stree::AbstractResourceReader::getAsVariant
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;

    bool accessAllowedToOwnDataOnly() const;

private:
    stree::ResourceContainer m_rc;
};

}   //cdb
}   //nx

#endif  //CLOUD_DB_AUTH_TYPES_H
