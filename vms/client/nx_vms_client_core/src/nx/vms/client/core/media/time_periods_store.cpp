// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "time_periods_store.h"

#include <QtQml/QtQml>

namespace nx::vms::client::core {

void TimePeriodsStore::registerQmlType()
{
    qmlRegisterType<nx::vms::client::core::TimePeriodsStore>("Nx.Core", 1, 0, "TimePeriodsStore");
}

TimePeriodsStore::TimePeriodsStore(QObject* parent):
    base_type(parent)
{
}

bool TimePeriodsStore::hasPeriods(Qn::TimePeriodContent type) const
{
    return !periods(type).isEmpty();
}

const QnTimePeriodList& TimePeriodsStore::periods(Qn::TimePeriodContent type) const
{
    NX_ASSERT(false, "Not implemented. Method cannot be pure virtual as class is used in QML");
    static QnTimePeriodList kEmptyPeriods;
    return kEmptyPeriods;
}

} // namespace nx::vms::client::core
