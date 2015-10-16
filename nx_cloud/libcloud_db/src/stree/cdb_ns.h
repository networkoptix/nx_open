/**********************************************************
* 6 may 2015
* a.kolesnikov
***********************************************************/


namespace nx {
namespace cdb {

//!Attributes to be used with stree
namespace attr {
enum Value
{
    operation = 1,

    //source data attributes
    accountID,
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
    authAccountID,
    authSystemID,

    entity,
    action,
    secureSource,

    socketIntfIP,
    socketRemoteIP,

    requestPath
};
}

}   //cdb
}   //nx
