
#include "abstract_statistics_storage.h"

#include <nx/utils/log/assert.h>

QnAbstractStatisticsStorage::QnAbstractStatisticsStorage()
{}

QnAbstractStatisticsStorage::~QnAbstractStatisticsStorage()
{}

void QnAbstractStatisticsStorage::storeMetrics(const QnStatisticValuesHash &/*metrics*/)
{
    NX_ASSERT(false, Q_FUNC_INFO, "Pure virtual method called!");
}

QnMetricHashesList QnAbstractStatisticsStorage::getMetricsList(qint64 /*startTimeMs*/
    , int /*limit*/) const
{
    NX_ASSERT(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QnMetricHashesList();
}

bool QnAbstractStatisticsStorage::saveCustomSettings(const QString &/*name*/
    , const QByteArray &/*settings*/)
{
    NX_ASSERT(false, Q_FUNC_INFO, "Pure virtual method called!");
    return false;
}

QByteArray QnAbstractStatisticsStorage::getCustomSettings(const QString &/*name*/) const
{
    NX_ASSERT(false, Q_FUNC_INFO, "Pure virtual method called!");
    return QByteArray();
}

void QnAbstractStatisticsStorage::removeCustomSettings(const QString &/*name*/)
{
    NX_ASSERT(false, Q_FUNC_INFO, "Pure virtual method called!");
}
