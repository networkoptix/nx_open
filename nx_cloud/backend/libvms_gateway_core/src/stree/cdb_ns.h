/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_ATTR_NS_H
#define NX_CDB_ATTR_NS_H

#include <nx/utils/stree/resourcenameset.h>


namespace nx {
namespace cloud {
namespace gateway {

//!Attributes to be used with stree
namespace attr {
enum Value
{
    operation = 1,

    //source data attributes
    accountID,
    accountEmail,
    ha1,
    userName,
    userPassword,
    accountStatus,
    systemAccessRole,

    systemID,
    systemStatus,   /** int */

    dataAccountRightsOnSystem,

    //authentication/authorization intermediate attributes
    authenticated,
    authorized,
    authAccountRightsOnSystem,
    authAccountEmail,
    authSystemID,
    /** Operation data contains accountID equal to the one been authenticated */
    authSelfAccountAccessRequested,
    /** Operation data contains systemID equal to the one been authenticated */
    authSelfSystemAccessRequested,
    /** request has been authenticated by code sent to account email */
    authenticatedByEmailCode,
    resultCode,

    entity,
    action,
    secureSource,

    socketIntfIP,
    socketRemoteIP,

    requestPath
};
}   //namespace attr

//!Contains description of stree attributes used by cloud_db
class CdbAttrNameSet
:
    public nx::utils::stree::ResourceNameSet
{
public:
    CdbAttrNameSet();
};

}   //namespace cloud
}   //namespace gateway
}   //namespace nx

#endif  //NX_CDB_ATTR_NS_H
