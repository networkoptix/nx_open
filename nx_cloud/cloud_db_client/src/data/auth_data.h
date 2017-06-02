#pragma once 

#include <QtCore/QUrlQuery>

#include <nx/fusion/model_functions_fwd.h>

#include <cdb/auth_provider.h>

namespace nx {
namespace cdb {
namespace api {

// TODO: #ak Add corresponding parser/serializer to fusion and remove this function.
bool loadFromUrlQuery(const QUrlQuery& urlQuery, AuthRequest* const authRequest);
void serializeToUrlQuery(const AuthRequest&, QUrlQuery* const urlQuery);

#define NonceData_Fields (nonce)(validPeriod)
#define AuthRequest_Fields (nonce)(username)(realm)
#define AuthResponse_Fields (nonce)(intermediateResponse) \
    (authenticatedAccountData)(accessRole)(validPeriod)
#define AuthInfoRecord_Fields (nonce)(intermediateResponse)(expirationTime)
#define AuthInfo_Fields (records)

extern const char* const kVmsUserAuthInfoAttributeName;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (NonceData)(AuthRequest)(AuthResponse)(AuthInfoRecord)(AuthInfo),
    (json))

} // namespace api
} // namespace cdb
} // namespace nx
