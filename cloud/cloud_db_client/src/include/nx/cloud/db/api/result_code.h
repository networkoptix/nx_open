// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/reflect/enum_instrument.h>

namespace nx::cloud::db::api {

static constexpr int kErrorCodeBase = 100;

NX_REFLECTION_ENUM_CLASS(ResultCode,
    ok,
    /**
     * Operation succeeded but not full data has been returned
     * (e.g., due to memory usage restrictions).
     * Caller should continue fetching data supplying data offset.
     */
    created,
    /** Operation succeeded, object was created */
    partialContent,
    /** Operation succeeded, but no content can be returned. */
    noContent,
    /** Provided credentials are invalid. */
    notAuthorized = kErrorCodeBase,

    /** Requested operation is not allowed with credentials provided. */
    forbidden,
    accountNotActivated,
    accountBlocked,
    secondFactorRequired,
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

    // Merge specific codes.

    mergedSystemIsOffline,
    vmsRequestFailure,

    /** Not all servers of the slave system have been merged into the target system. */
    mergeCompletedPartially,

    invalidTotp,
    invalidBackupCode,
    userPasswordRequired,

    /** Credentials used for authentication are no longer valid. */
    credentialsRemovedPermanently,

    /** Received data in unexpected/unsupported format. */
    invalidFormat,
    retryLater,

    /** Data constraints violation */
    updateConflict,

    notModified,

    tooManyRequests,

    unknownError
);

std::string toString(ResultCode resultCode);
bool isSuccessCode(ResultCode code);

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
    bool isTechnicalError();
    bool isSuccess() const { return isSuccessCode(code); }
    bool isBusinessLogicError();

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

    bool operator==(ResultCode right) const
    {
        return code == right;
    }
};

} // namespace nx::cloud::db::api
