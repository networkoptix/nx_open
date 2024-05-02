// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <vector>

#include <nx/sdk/cloud_storage/i_time_periods.h>
#include <nx/sdk/helpers/ref_countable.h>

#include "data.h"

namespace nx::sdk::cloud_storage {

class TimePeriods: public nx::sdk::RefCountable<ITimePeriods>
{
public:
    TimePeriods() = default;
    TimePeriods(const std::vector<nx::sdk::cloud_storage::TimePeriod>& periods);

    void setPeriods(const std::vector<nx::sdk::cloud_storage::TimePeriod>& periods);

    virtual void goToBeginning() const override;
    virtual bool next() const override;
    virtual bool atEnd() const override;
    virtual bool get(int64_t* outStartUs, int64_t* outEndUs) const override;

private:
    mutable std::vector<TimePeriod> m_periods;
    mutable std::vector<TimePeriod>::const_iterator m_it;
};

} // namespace nx::sdk::cloud_storage
