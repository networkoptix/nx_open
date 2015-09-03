/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_RESULT_CODE_H
#define NX_CDB_RESULT_CODE_H


namespace nx {
namespace cdb {
namespace api {

enum class ResultCode
{
    ok,
    //!Provided credentials are invalid
    notAuthorized,
    //!Requested operation is not allowed with credentials provided
    forbidden,
    notFound,
    alreadyExists,
    dbError,
    //!Network operation failed
    networkError,
    notImplemented
};

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_RESULT_CODE_H
