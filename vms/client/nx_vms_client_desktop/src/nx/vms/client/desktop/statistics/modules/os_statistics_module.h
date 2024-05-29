// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <statistics/abstract_statistics_module.h>

namespace nx::vms::client::desktop {

/** OS statistics, never changed during application lifetime. */
class OsStatisticsModule: public QnAbstractStatisticsModule
{
    using base_type = QnAbstractStatisticsModule;

public:
    virtual QnStatisticValuesHash values() const override;
};

} // namespace nx::vms::client::desktop
