#include "error.h"

#define NX_PRINT_PREFIX "nx::sdk::error() "
#include <nx/sdk/helpers/string.h>

#include <nx/kit/debug.h>

namespace nx {
namespace sdk {

Error error(ErrorCode errorCode, std::string errorMessage)
{
    NX_KIT_ASSERT(errorCode != ErrorCode::noError,
        "Error code must differ from `ErrorCode::noError`");
    NX_KIT_ASSERT(!errorMessage.empty(), "Error message must not be empty");

    return { errorCode, new String(std::move(errorMessage)) };
}

} // namespace sdk
} // namespace nx
