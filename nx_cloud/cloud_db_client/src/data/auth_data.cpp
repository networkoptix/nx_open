/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#include "auth_data.h"

#include <utils/common/model_functions.h>

#include "system_data.h"


namespace nx {
namespace cdb {
namespace api {

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AuthRequest* const authRequest)
{
    if (!urlQuery.hasQueryItem("nonce") ||
        !urlQuery.hasQueryItem("username") ||
        !urlQuery.hasQueryItem("realm"))
    {
        return false;
    }

    authRequest->nonce = urlQuery.queryItemValue("nonce").toStdString();
    authRequest->username = urlQuery.queryItemValue("username").toStdString();
    authRequest->realm = urlQuery.queryItemValue("realm").toStdString();
    return true;
}

void serializeToUrlQuery(const AuthRequest& authRequest, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem("nonce", QString::fromStdString(authRequest.nonce));
    urlQuery->addQueryItem("username", QString::fromStdString(authRequest.username));
    urlQuery->addQueryItem("realm", QString::fromStdString(authRequest.realm));
}


QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (NonceData)(AuthRequest)(AuthResponse),
    (json),
    _Fields)

}   //api
}   //cdb
}   //nx
