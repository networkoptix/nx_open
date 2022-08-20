// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_time_period_storage.h"

#include <QtQml/QtQml>

#include <recording/time_period_list.h>

namespace nx::vms::client::core {

void AbstractTimePeriodStorage::registerQmlType()
{
    qmlRegisterType<AbstractTimePeriodStorage>(
        "nx.vms.client.core", 1, 0, "AbstractTimePeriodStorage");
}

AbstractTimePeriodStorage::AbstractTimePeriodStorage(QObject* parent):
    base_type(parent)
{
}

bool AbstractTimePeriodStorage::hasPeriods(Qn::TimePeriodContent type) const
{
    return !periods(type).isEmpty();
}

const QnTimePeriodList& AbstractTimePeriodStorage::periods(Qn::TimePeriodContent /* type */) const
{
    NX_ASSERT(false, "Not implemented. Method cannot be pure virtual as class is used in QML");
    static QnTimePeriodList kEmptyPeriods;
    return kEmptyPeriods;
}

} // namespace nx::vms::client::core
