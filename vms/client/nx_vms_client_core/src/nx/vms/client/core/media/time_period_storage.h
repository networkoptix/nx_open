// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <nx/vms/client/core/media/abstract_time_period_storage.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>

namespace nx::vms::client::core {

/**
 * Class for collecting, aggregating and providing time period lists.
 */
class NX_VMS_CLIENT_CORE_API TimePeriodStorage: public AbstractTimePeriodStorage
{
public:
    TimePeriodStorage(QObject* parent = nullptr);

    virtual const QnTimePeriodList& periods(Qn::TimePeriodContent type) const override;
    QnTimePeriodList aggregated(Qn::TimePeriodContent type) const;
    void setPeriods(Qn::TimePeriodContent type, const QnTimePeriodList& timePeriods);

    int aggregationMSecs() const;
    void setAggregationMSecs(int value);

protected:
    void updateAggregated(Qn::TimePeriodContent type);

private:
    QnTimePeriodList m_normalPeriods[Qn::TimePeriodContentCount];
    QnTimePeriodList m_aggregatedPeriods[Qn::TimePeriodContentCount];
    int m_aggregationMSecs = 0;
};

} // namespace nx::vms::client::core
