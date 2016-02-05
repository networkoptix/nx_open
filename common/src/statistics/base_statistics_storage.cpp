
#include "base_statistics_storage.h"

QnBaseStatisticsStorage::QnBaseStatisticsStorage(QObject *parent)
    : base_type(parent)
{}

QnBaseStatisticsStorage::~QnBaseStatisticsStorage()
{}

void QnBaseStatisticsStorage::storeMetrics(const QnMetricsHash &metrics)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
}

QnMetricHashesList QnBaseStatisticsStorage::getMetricsList(qint64 startTimeMs
    , int limit) const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QnMetricHashesList();
}

bool QnBaseStatisticsStorage::saveCustomSettings(const QString &name
    , const QByteArray &settings)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return false;
}

QByteArray QnBaseStatisticsStorage::getCustomSettings(const QString &name) const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QByteArray();
}

void QnBaseStatisticsStorage::removeCustomSettings(const QString &name)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual method called!");
}