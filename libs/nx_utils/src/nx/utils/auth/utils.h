// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/buffer.h>

namespace nx::utils::auth {

NX_UTILS_API Buffer hmacSha1(const std::string_view& key, const std::string_view& message);

NX_UTILS_API Buffer hmacSha1(
    const std::string_view& key,
    const std::vector<std::string_view>& messageParts);

NX_UTILS_API Buffer hmacSha256(const std::string_view& key, const std::string_view& message);

NX_UTILS_API std::string encodeToBase32(const std::string& str);
NX_UTILS_API std::tuple<bool, std::string> decodeBase32(const std::string& str);

} // namespace nx::utils::auth
