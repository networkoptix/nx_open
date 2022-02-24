// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/buffer.h>

namespace nx::utils::auth {

Buffer NX_UTILS_API hmacSha1(const std::string_view& publicKey, const std::string_view& secret);
NX_UTILS_API Buffer hmacSha256(const std::string_view& publicKey, const std::string_view& secret);

std::string NX_UTILS_API encodeToBase32(const std::string& str);
std::tuple<bool, std::string> NX_UTILS_API decodeBase32(const std::string& str);

} // namespace nx::utils::auth
