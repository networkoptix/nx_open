// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/sdk/result.h>

namespace nx {
namespace sdk {

Error error(ErrorCode errorCode, std::string errorMessage);

} // namespace sdk
} // namespace nx
