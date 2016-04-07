/**********************************************************
* Sep 3, 2015
* NetworkOptix
* akolesnikov
***********************************************************/

#ifndef NX_CDB_API_RESULT_CODE_H
#define NX_CDB_API_RESULT_CODE_H

#include <string>


namespace nx {
namespace cdb {
namespace api {

static const int CDB_API_ERROR_CODE_BASE = 100;

enum class ResultCode
{
    ok = 0,
    /** Provided credentials are invalid */
    notAuthorized = CDB_API_ERROR_CODE_BASE,

    /** Requested operation is not allowed with credentials provided */
    forbidden,
    accountNotActivated,
    accountBlocked,

    notFound,
    alreadyExists,
    dbError,
    /** Network operation failed */
    networkError,
    notImplemented,
    unknownRealm,
    badUsername,
    badRequest,
    invalidNonce,
    serviceUnavailable,

    /** Credentials used for authentication are no longer valid */
    credentialsRemovedPermanently,

    unknownError
};

std::string toString(ResultCode resultCode);

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_API_RESULT_CODE_H
