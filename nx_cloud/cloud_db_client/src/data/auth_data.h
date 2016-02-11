/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_AUTH_DATA_H
#define NX_CDB_CL_AUTH_DATA_H

#include <QtCore/QUrlQuery>

#include <utils/common/model_functions_fwd.h>

#include <cdb/auth_provider.h>


namespace nx {
namespace cdb {
namespace api {

//TODO #ak add corresponding parser/serializer to fusion and remove this function
bool loadFromUrlQuery(const QUrlQuery& urlQuery, AuthRequest* const authRequest);
void serializeToUrlQuery(const AuthRequest&, QUrlQuery* const urlQuery);

#define NonceData_Fields (nonce)(validPeriod)
#define AuthRequest_Fields (nonce)(username)(realm)
#define AuthResponse_Fields (nonce)(intermediateResponse)(accessRole)(validPeriod)


QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (NonceData)(AuthRequest)(AuthResponse),
    (json) )


}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_CL_AUTH_DATA_H
