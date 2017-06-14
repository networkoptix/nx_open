#include <vector>
#include <gtest/gtest.h>

#include <nx/cloud/cdb/api/result_code.h>
#include <cloud/cloud_result_info.h>

using namespace nx::cdb::api;

#define NX_CDB_RESULT_CODE_LIST(APPLY) \
    APPLY(ok) \
    /**
     * Operation succeeded but not full data has been returned
     * (e.g., due to memory usage restrictions).
     * Caller should continue fetching data supplying data offset.
     */ \
    APPLY(partialContent) \
    /** Provided credentials are invalid. */ \
    APPLY(notAuthorized) \
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

#define NX_CDB_RESULT_CODE_TEST_CASE_APPLY(name) case ResultCode::name: { int i = 5; (void)i; } break;
#define NX_CDB_RESULT_CODE_TEST_ASSERT_APPLY(name) ASSERT_FALSE(QnCloudResultInfo::toString(ResultCode::name).isEmpty());

TEST(CdbResultCodeTest, ToString)
{
    ResultCode resultCode(ResultCode::ok);

    /* This dummy switch should produce compiler warning (error in case of -Werror=switch) if 
       new enum value is added and not propagated to this test. */
    switch(resultCode)
    {
        NX_CDB_RESULT_CODE_LIST(NX_CDB_RESULT_CODE_TEST_CASE_APPLY)
    }

    /* QnCloudResultInfo::toString() function result test */
    NX_CDB_RESULT_CODE_LIST(NX_CDB_RESULT_CODE_TEST_ASSERT_APPLY)
}
