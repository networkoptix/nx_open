// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/time/abstract_time_provider.h>

namespace nx::utils::time {

class NX_VMS_COMMON_API TimeProvider: public AbstractTimeProvider
{
public:
    virtual std::chrono::microseconds currentTime() const override;
};

} // namespace nx::utils::time
