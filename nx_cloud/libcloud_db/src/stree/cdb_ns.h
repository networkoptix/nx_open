/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_ATTR_NS_H
#define NX_CDB_ATTR_NS_H

#include <plugins/videodecoder/stree/resourcenameset.h>


namespace nx {
namespace cdb {

//!Attributes to be used with stree
namespace attr {
enum Value
{
    operation = 1,

    //source data attributes
    accountID,
    accountEmail,
    systemID,
    ha1,
    userName,
    userPassword,
    accountStatus,
    systemAccessRole,

    dataAccountRightsOnSystem,

    //authentication/authorization intermediate attributes
    authenticated,
    authorized,
    authAccountRightsOnSystem,
    authAccountEmail,
    authSystemID,
    //!Operation data contains accountID equal to the one been authenticated
    authSelfAccountAccessRequested,

    entity,
    action,
    secureSource,

    socketIntfIP,
    socketRemoteIP,

    requestPath
};
}

//!Contains description of stree attributes used by cloud_db
class CdbAttrNameSet
:
    public stree::ResourceNameSet
{
public:
    CdbAttrNameSet();
};

}   //cdb
}   //nx

#endif  //NX_CDB_ATTR_NS_H
