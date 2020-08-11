#pragma once

#include <nx/utils/time/abstract_time_provider.h>

namespace nx::utils::time {

class TimeProvider: public AbstractTimeProvider
{
public:
    virtual std::chrono::microseconds currentTime() const override;
};

} // namespace nx::utils::time
