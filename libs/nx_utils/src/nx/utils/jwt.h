// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx::utils {

inline std::string jwtWithoutSignature(const std::string& str)
{
    const auto pos = str.find_last_of('.');
    if (pos == std::string::npos)
        return ""; // not a valid JWT
    return str.substr(0, pos);
}

} // namespace nx::utils
