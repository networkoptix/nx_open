// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/log_main.h>

namespace nx::vms::client::core {

#define NX_LOG_RESPONSE(TAG, success, response, ...) do \
{ \
    const auto& nxLogResponseSuccess = (success); \
    const auto& nxLogResponseResponse = (response); \
    if (!nxLogResponseSuccess || !nxLogResponseResponse) \
    { \
        NX_WARNING(TAG, __VA_ARGS__); \
        if (nxLogResponseResponse) \
        { \
            NX_WARNING(TAG, "Success: %1, Response: [OK]", nxLogResponseSuccess); \
        } \
        else \
        { \
            const auto& nxLogResponseError = nxLogResponseResponse.error(); \
            NX_WARNING(TAG, "Success: %1, Error: %2, %3", \
                nxLogResponseSuccess, \
                nxLogResponseError.errorId, \
                nxLogResponseError.errorString); \
        } \
    } \
} while (0)

} // namespace nx::vms::client::core
