
#include "abstract_statistics_storage.h"

QnAbstractStatisticsStorage::QnAbstractStatisticsStorage()
{}

QnAbstractStatisticsStorage::~QnAbstractStatisticsStorage()
{}

void QnAbstractStatisticsStorage::storeMetrics(const QnStatisticValuesHash &metrics)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
}

QnMetricHashesList QnAbstractStatisticsStorage::getMetricsList(qint64 startTimeMs
    , int limit) const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QnMetricHashesList();
}

bool QnAbstractStatisticsStorage::saveCustomSettings(const QString &name
    , const QByteArray &settings)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return false;
}

QByteArray QnAbstractStatisticsStorage::getCustomSettings(const QString &name) const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QByteArray();
}

void QnAbstractStatisticsStorage::removeCustomSettings(const QString &name)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
}