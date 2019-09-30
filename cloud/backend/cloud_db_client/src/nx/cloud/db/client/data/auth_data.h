#pragma once

#include <QtCore/QUrlQuery>

#include <nx/fusion/model_functions_fwd.h>

#include <nx/cloud/db/api/auth_provider.h>

namespace nx::cloud::db::api {

// TODO: #ak Add corresponding parser/serializer to fusion and remove this function.
bool loadFromUrlQuery(const QUrlQuery& urlQuery, AuthRequest* const authRequest);
void serializeToUrlQuery(const AuthRequest&, QUrlQuery* const urlQuery);

#define NonceData_Fields (nonce)(validPeriod)
#define AuthRequest_Fields (nonce)(username)(realm)
#define AuthResponse_Fields (nonce)(intermediateResponse) \
    (authenticatedAccountData)(accessRole)(validPeriod)

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(nx::cloud::db::api::ObjectType)

#define UserAuthorization_Fields (requestMethod)(requestAuthorization)
#define CredentialsDescriptor_Fields (status)(objectType)(objectId)
#define SystemAccess_Fields (accessRole)

#define AuthInfoRecord_Fields (nonce)(intermediateResponse)(expirationTime)
#define AuthInfo_Fields (records)

static constexpr char kVmsUserAuthInfoAttributeName[] = "cloudUserAuthenticationInfo";

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (NonceData)(AuthRequest)(AuthResponse) \
        (UserAuthorization)(CredentialsDescriptor)(SystemAccess) \
        (AuthInfoRecord)(AuthInfo),
    (json))

} // namespace nx::cloud::db::api

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((nx::cloud::db::api::ObjectType), (lexical))
