#pragma once

#include <string>

namespace nx {
namespace cdb {
namespace api {

static const int kErrorCodeBase = 100;

enum class ResultCode
{
    ok = 0,
    notAuthorized = kErrorCodeBase, //< Provided credentials are invalid.
    forbidden, //< Requested operation is not allowed with credentials provided.
    accountNotActivated,
    accountBlocked,
    notFound,
    alreadyExists,
    dbError,
    networkError, //< Network operation failed.
    notImplemented,
    unknownRealm,
    badUsername,
    badRequest,
    invalidNonce,
    serviceUnavailable,
    credentialsRemovedPermanently, //< Credentials used for authentication are no longer valid.
    invalidFormat, //< Received data in unexpected/unsupported format.
    unknownError
};

std::string toString(ResultCode resultCode);

} // namespace api
} // namespace cdb
} // namespace nx
