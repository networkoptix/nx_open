/**********************************************************
* Sep 3, 2015
* NetworkOptix
* akolesnikov
***********************************************************/

#ifndef NX_CDB_API_RESULT_CODE_H
#define NX_CDB_API_RESULT_CODE_H


namespace nx {
namespace cdb {
namespace api {

static const int CDB_API_ERROR_CODE_BASE = 100;

enum class ResultCode
{
    ok = 0,
    //!Provided credentials are invalid
    notAuthorized = CDB_API_ERROR_CODE_BASE,
    //!Requested operation is not allowed with credentials provided
    forbidden,
    notFound,
    alreadyExists,
    dbError,
    //!Network operation failed
    networkError,
    notImplemented,
    unknownRealm,
    badUsername,
    badRequest,
    invalidNonce,
    serviceUnavailable,
    unknownError
};

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_API_RESULT_CODE_H
