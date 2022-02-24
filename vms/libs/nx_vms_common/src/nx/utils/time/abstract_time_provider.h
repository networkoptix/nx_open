// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

namespace nx::utils::time {

class AbstractTimeProvider
{
public:
    virtual ~AbstractTimeProvider() {}

    virtual std::chrono::microseconds currentTime() const = 0;
};

} // namespace nx::utils::time
