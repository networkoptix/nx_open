#pragma once

#include <nx/fusion/model_functions_fwd.h>

struct DetachFromCloudReply
{
    enum ResultCode
    {
        ok = 0,
        invalidPasswordData,
        cannotUpdateUserCredentials,
        errorFromCloudServer, //< Details in cloudResultCode.
        cannotCleanUpCloudDataInLocalDb,
    };

    DetachFromCloudReply():
        resultCode(ResultCode::ok),
        cloudServerResultCode(0)
    {
    }

    DetachFromCloudReply(
        ResultCode resultCode,
        int cloudServerResultCode = 0)
        :
        resultCode(resultCode),
        cloudServerResultCode(cloudServerResultCode)
    {
    }

    ResultCode resultCode;
    int cloudServerResultCode; //< Value is taken from nx::cdb::api::ResultCode; 0 means no error.
};
#define DetachFromCloudReply_Fields (resultCode)(cloudServerResultCode)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((DetachFromCloudReply), (json))
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(DetachFromCloudReply::ResultCode)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((DetachFromCloudReply::ResultCode), (numeric))