#pragma once

//#include <string>

namespace nx {
namespace cdb {
namespace api {

static const int kErrorCodeBase = 100;

#define NX_CDB_RESULT_CODE_LIST(APPLY) \
    APPLY(ok, 0) \
    /**
     * Operation succeeded but not full data has been returned
     * (e.g., due to memory usage restrictions).
     * Caller should continue fetching data supplying data offset.
     */ \
    APPLY(partialContent) \
    /** Provided credentials are invalid. */ \
    APPLY(notAuthorized, kErrorCodeBase) \
\
    /** Requested operation is not allowed with credentials provided. */ \
    APPLY(forbidden) \
    APPLY(accountNotActivated) \
    APPLY(accountBlocked) \
    APPLY(notFound) \
    APPLY(alreadyExists) \
    APPLY(dbError) \
    APPLY(networkError) /*< Network operation failed.*/ \
    APPLY(notImplemented) \
    APPLY(unknownRealm) \
    APPLY(badUsername) \
    APPLY(badRequest) \
    APPLY(invalidNonce) \
    APPLY(serviceUnavailable) \
    \
    /** Credentials used for authentication are no longer valid. */ \
    APPLY(credentialsRemovedPermanently) \
    \
    /** Received data in unexpected/unsupported format. */ \
    APPLY(invalidFormat) \
    APPLY(retryLater) \
\
    APPLY(unknownError)

#define NX_CDB_RESULT_CODE_APPLY_1(name) name,
#define NX_CDB_RESULT_CODE_APPLY_2(name, value) name = value,

#define NX_CDB_RESULT_CODE_EXPAND(v) v
#define NX_CDB_RESULT_CODE_APPLY_IMPL(_1, _2, _3, ...) _3

#define NX_CDB_RESULT_CODE_APPLY(...) \
    NX_CDB_RESULT_CODE_EXPAND( \
        NX_CDB_RESULT_CODE_APPLY_IMPL( \
            __VA_ARGS__, \
            NX_CDB_RESULT_CODE_APPLY_2, \
            NX_CDB_RESULT_CODE_APPLY_1) (__VA_ARGS__))

enum class ResultCode
{
NX_CDB_RESULT_CODE_LIST(NX_CDB_RESULT_CODE_APPLY)
};

std::string toString(ResultCode resultCode);

} // namespace api
} // namespace cdb
} // namespace nx
