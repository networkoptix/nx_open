// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "auth_data.h"

#include <nx/fusion/model_functions.h>

#include "system_data.h"
#include "types.h"
#include "../field_name.h"

namespace nx::cloud::db::api {

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(NonceData, (json), NonceData_Fields, (optional, false))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AuthRequest, (json), AuthRequest_Fields, (optional, false))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AuthResponse, (json), AuthResponse_Fields, (optional, false))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserAuthorization, (json), UserAuthorization_Fields, (optional, false))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    UserAuthorizationList, (json), UserAuthorizationList_Fields, (optional, false))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CredentialsDescriptor, (json), CredentialsDescriptor_Fields, (optional, false))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CredentialsDescriptorList, (json), CredentialsDescriptorList_Fields, (optional, false))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemAccess, (json), SystemAccess_Fields, (optional, false))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AuthInfoRecord, (json), AuthInfoRecord_Fields, (optional, false))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AuthInfo, (json), AuthInfo_Fields, (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(VmsServerCertificatePublicKey,
    (json),
    VmsServerCertificatePublicKey_Fields,
    (optional, false))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemNonce, (ubjson)(json), SystemNonce_Fields)

} // namespace nx::cloud::db::api
