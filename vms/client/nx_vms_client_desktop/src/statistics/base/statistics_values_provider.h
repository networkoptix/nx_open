// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <statistics/statistics_fwd.h>

// @brief Represents provider for statistics values
class QnStatisticsValuesProvider
{
public:
    QnStatisticsValuesProvider() {}

    virtual ~QnStatisticsValuesProvider() {}

    virtual QnStatisticValuesHash values() const = 0;

    virtual void reset() = 0;
};
