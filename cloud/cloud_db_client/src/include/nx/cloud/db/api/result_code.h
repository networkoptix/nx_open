// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx::cloud::db::api {

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

struct Result
{
    ResultCode code = ResultCode::ok;
    std::string description;

    Result() = default;

    Result(ResultCode code, std::string description = std::string()):
        code(code),
        description(description)
    {
    }

    bool ok() const { return code == ResultCode::ok; }

    std::string toString() const
    {
        return api::toString(code) + " " + description;
    }

    Result& operator=(ResultCode newCode)
    {
        code = newCode;
        description = api::toString(code);
        return *this;
    }
};

inline bool operator==(const Result& left, ResultCode right)
{ return left.code == right; }

inline bool operator!=(const Result& left, ResultCode right)
{ return !(left == right); }

inline bool operator==(ResultCode left, const Result& right)
{ return left == right.code; }

inline bool operator!=(ResultCode left, const Result& right)
{ return !(left == right); }

} // namespace nx::cloud::db::api
