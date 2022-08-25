// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <chrono>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/cloud_storage/i_time_periods.h>

#include "data.h"

namespace nx {
namespace sdk {
namespace cloud_storage {

class TimePeriods: public nx::sdk::RefCountable<ITimePeriods>
{
public:
    TimePeriods(const TimePeriodList& periods);

    virtual void goToBeginning() const override;
    virtual bool next() const override;
    virtual bool atEnd() const override;
    virtual bool get(int64_t* outStartUs, int64_t* outEndUs) const override;

private:
    mutable TimePeriodList m_periods;
    mutable TimePeriodList::const_iterator m_it;
};

} // namespace cloud_storage
} // namespace sdk
} // namespace nx

