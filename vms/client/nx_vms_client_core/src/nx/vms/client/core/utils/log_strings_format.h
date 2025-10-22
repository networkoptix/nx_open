// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/log/log_main.h>

namespace nx::vms::client::core {

#define NX_LOG_RESPONSE(TAG, success, response, ...) \
    if (!success || !response) \
    { \
        NX_WARNING(TAG, __VA_ARGS__); \
        if (response) \
        { \
            NX_WARNING(TAG, "Success: %1, Response: [OK]", success); \
        } \
        else \
        { \
            NX_WARNING(TAG, "Success: %1, Error: %2, %3", \
                success, response.error().errorId, response.error().errorString); \
        } \
    }

} // namespace nx::vms::client::core
