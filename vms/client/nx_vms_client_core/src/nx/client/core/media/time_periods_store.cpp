#include "time_periods_store.h"

#include <QtQml/QtQml>

namespace nx::client::core {

void TimePeriodsStore::registerQmlType()
{
    qmlRegisterType<nx::client::core::TimePeriodsStore>("Nx.Core", 1, 0, "TimePeriodsStore");
}

TimePeriodsStore::TimePeriodsStore(QObject* parent):
    base_type(parent)
{
}

bool TimePeriodsStore::hasPeriods(Qn::TimePeriodContent type) const
{
    return !m_periods[type].isEmpty();
}

void TimePeriodsStore::setPeriods(Qn::TimePeriodContent type, const QnTimePeriodList& periods)
{
    m_periods.insert(type, periods);
    emit periodsUpdated(type);
}

QnTimePeriodList TimePeriodsStore::periods(Qn::TimePeriodContent type) const
{
    return m_periods[type];
}

} // namespace nx::client::core
