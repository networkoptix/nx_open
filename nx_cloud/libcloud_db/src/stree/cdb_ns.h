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
    accountID,
    systemID,
    authenticated,
    authorized,
    ha1,
    userName,
    userPassword,
    authAccountRightsOnSystem,

    socketIntfIP,
    socketRemoteIP,

    requestPath
};
}

}   //cdb
}   //nx
