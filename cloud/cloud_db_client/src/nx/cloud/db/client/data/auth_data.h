// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrlQuery>

#include <nx/reflect/instrument.h>

#include <nx/cloud/db/api/auth_provider.h>

namespace nx::cloud::db::api {

// TODO: #akolesnikov Add corresponding parser/serializer to fusion and remove this function.
bool loadFromUrlQuery(const QUrlQuery& urlQuery, AuthRequest* const authRequest);
void serializeToUrlQuery(const AuthRequest&, QUrlQuery* const urlQuery);

#define NonceData_Fields (nonce)(validPeriod)

NX_REFLECTION_INSTRUMENT(NonceData, NonceData_Fields)

#define AuthRequest_Fields (nonce)(username)(realm)

NX_REFLECTION_INSTRUMENT(AuthRequest, AuthRequest_Fields)

#define AuthResponse_Fields (nonce)(intermediateResponse) \
    (authenticatedAccountData)(accessRole)(validPeriod)

NX_REFLECTION_INSTRUMENT(AuthResponse, AuthResponse_Fields)

#define UserAuthorization_Fields (requestMethod)(requestAuthorization)(baseNonce)

NX_REFLECTION_INSTRUMENT(UserAuthorization, UserAuthorization_Fields)

#define UserAuthorizationList_Fields (authorizations)

NX_REFLECTION_INSTRUMENT(UserAuthorizationList, UserAuthorizationList_Fields)

NX_REFLECTION_INSTRUMENT_ENUM(ObjectType, none, account, system)

#define CredentialsDescriptor_Fields (status)(objectType)(objectId)(intermediateResponse)

NX_REFLECTION_INSTRUMENT(CredentialsDescriptor, CredentialsDescriptor_Fields)

#define CredentialsDescriptorList_Fields (descriptors)

NX_REFLECTION_INSTRUMENT(CredentialsDescriptorList, CredentialsDescriptorList_Fields)

#define SystemAccess_Fields (accessRole)

NX_REFLECTION_INSTRUMENT(SystemAccess, SystemAccess_Fields)

#define AuthInfoRecord_Fields (nonce)(intermediateResponse)(expirationTime)

NX_REFLECTION_INSTRUMENT(AuthInfoRecord, AuthInfoRecord_Fields)

#define AuthInfo_Fields (records)(twofaEnabled)

NX_REFLECTION_INSTRUMENT(AuthInfo, AuthInfo_Fields)

#define VmsServerCertificatePublicKey_Fields (key)

NX_REFLECTION_INSTRUMENT(
    VmsServerCertificatePublicKey,
    VmsServerCertificatePublicKey_Fields)

#define SystemNonce_Fields (systemId)(nonce)

NX_REFLECTION_INSTRUMENT(SystemNonce, SystemNonce_Fields)

static constexpr char kVmsUserAuthInfoAttributeName[] = "cloudUserAuthenticationInfo";

} // namespace nx::cloud::db::api
