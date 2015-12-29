
#include "abstract_statistics_metric.h"

using namespace statistics;

QnAbstractStatisticsMetric::QnAbstractStatisticsMetric(const QString &alias)
    : m_alias(alias)
    , m_isTurnedOn(false)
{
}

QnAbstractStatisticsMetric::~QnAbstractStatisticsMetric()
{
}

void QnAbstractStatisticsMetric::fixValue()
{
}

void QnAbstractStatisticsMetric::setTurnedOn(bool turnOn)
{
    if (m_isTurnedOn == turnOn)
        return;

    m_isTurnedOn = turnOn;
    emit turnedOnChanged();
}

bool QnAbstractStatisticsMetric::isTurnedOn()
{
    return m_isTurnedOn;
}

QString QnAbstractStatisticsMetric::alias() const
{
    return m_alias;
}

//

