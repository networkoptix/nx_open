#pragma once

#include <string>

namespace nx {
namespace cdb {
namespace api {

static const int kErrorCodeBase = 100;

enum class ResultCode
{
    ok = 0,
    /**
     * Operation succeeded but not full data has been returned 
     * (e.g., due to memory usage restrictions).
     * Caller should continue fetching data supplying data offset.
     */
    partialContent,
    /** Provided credentials are invalid. */
    notAuthorized = kErrorCodeBase,

    /** Requested operation is not allowed with credentials provided. */
    forbidden,
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
    notAllowedInCurrentState,
    mergedSystemIsOffline,
    vmsRequestFailure,

    /** Credentials used for authentication are no longer valid. */
    credentialsRemovedPermanently,

    /** Received data in unexpected/unsupported format. */
    invalidFormat,
    retryLater,

    unknownError
};

std::string toString(ResultCode resultCode);

} // namespace api
} // namespace cdb
} // namespace nx
