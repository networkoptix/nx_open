/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#include "auth_data.h"

#include <utils/common/model_functions.h>


namespace nx {
namespace cdb {
namespace api {

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AuthRequest* const authRequest)
{
    //TODO #ak
    return false;
}

void serializeToUrlQuery(const AuthRequest& authRequest, QUrlQuery* const urlQuery)
{
    //TODO #ak
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (NonceData)(AuthRequest)(AuthResponse),
    (json),
    _Fields)


}   //api
}   //cdb
}   //nx
