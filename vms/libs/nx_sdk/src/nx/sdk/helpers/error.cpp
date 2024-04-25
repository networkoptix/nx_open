// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "error.h"

#include <nx/sdk/helpers/string.h>

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX "nx::sdk::error(): "
#include <nx/kit/debug.h>

namespace nx::sdk {

Error error(ErrorCode errorCode, std::string errorMessage)
{
    NX_KIT_ASSERT(errorCode != ErrorCode::noError,
        "Error code must differ from `ErrorCode::noError`");
    NX_KIT_ASSERT(!errorMessage.empty(), "Error message must not be empty");

    return {errorCode, new String(std::move(errorMessage))};
}

} // namespace nx::sdk
