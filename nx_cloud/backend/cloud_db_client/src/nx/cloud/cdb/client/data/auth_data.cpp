#include "auth_data.h"

#include <nx/fusion/model_functions.h>

#include "system_data.h"
#include "../field_name.h"

namespace nx {
namespace cdb {
namespace api {

MAKE_FIELD_NAME_STR_CONST(AuthRequest, nonce)
MAKE_FIELD_NAME_STR_CONST(AuthRequest, username)
MAKE_FIELD_NAME_STR_CONST(AuthRequest, realm)

bool loadFromUrlQuery(const QUrlQuery& urlQuery, AuthRequest* const authRequest)
{
    if (!urlQuery.hasQueryItem(AuthRequest_nonce_field) ||
        !urlQuery.hasQueryItem(AuthRequest_username_field) ||
        !urlQuery.hasQueryItem(AuthRequest_realm_field))
    {
        return false;
    }

    authRequest->nonce = urlQuery.queryItemValue(AuthRequest_nonce_field).toStdString();
    authRequest->username = urlQuery.queryItemValue(AuthRequest_username_field).toStdString();
    authRequest->realm = urlQuery.queryItemValue(AuthRequest_realm_field).toStdString();
    return true;
}

void serializeToUrlQuery(const AuthRequest& authRequest, QUrlQuery* const urlQuery)
{
    urlQuery->addQueryItem(
        AuthRequest_nonce_field,
        QString::fromStdString(authRequest.nonce));
    urlQuery->addQueryItem(
        AuthRequest_username_field,
        QString::fromStdString(authRequest.username));
    urlQuery->addQueryItem(
        AuthRequest_realm_field,
        QString::fromStdString(authRequest.realm));
}

const char* const kVmsUserAuthInfoAttributeName = "cloudUserAuthenticationInfo";

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (NonceData)(AuthRequest)(AuthResponse)(AuthInfoRecord)(AuthInfo),
    (json),
    _Fields,
    (optional, false))

} // namespace api
} // namespace cdb
} // namespace nx
