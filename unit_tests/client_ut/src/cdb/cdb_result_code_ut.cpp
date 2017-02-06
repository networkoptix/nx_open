#include <gtest/gtest.h>

#include <cdb/result_code.h>
#include <cloud/cloud_result_info.h>

using namespace nx::cdb::api;

#define NX_CDB_TEST_RESULT_CODE_APPLY_1(name) ASSERT_FALSE(QnCloudResultInfo::toString(ResultCode::name).isEmpty());
#define NX_CDB_TEST_RESULT_CODE_APPLY_2(name, value) ASSERT_FALSE(QnCloudResultInfo::toString(ResultCode::name).isEmpty());

#define NX_CDB_RESULT_CODE_APPLY_IMPL(_1, _2, _3, ...) _3

#define NX_CDB_TEST_RESULT_CODE_APPLY(...) \
    NX_CDB_RESULT_CODE_EXPAND( \
        NX_CDB_RESULT_CODE_APPLY_IMPL( \
            __VA_ARGS__, \
            NX_CDB_TEST_RESULT_CODE_APPLY_2, \
            NX_CDB_TEST_RESULT_CODE_APPLY_1) (__VA_ARGS__))

TEST(CDBResultCode, main)
{
    NX_CDB_RESULT_CODE_LIST(NX_CDB_TEST_RESULT_CODE_APPLY)
}
