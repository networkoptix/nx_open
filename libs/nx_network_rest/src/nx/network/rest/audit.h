// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "user_access_data.h"

namespace nx::network::rest::audit {

struct Record: UserSession
{
    Record(Record&&) = default;
    Record(const Record&) = default;
    Record& operator=(Record&&) = default;
    Record& operator=(const Record&) = default;
    bool operator==(const Record&) const = default;

    Record(UserSession userSession): UserSession(std::move(userSession)) {}
};

} // namespace nx::network::rest::audit
