
#include "abstract_statistics_module.h"

QnAbstractStatisticsModule::QnAbstractStatisticsModule(QObject *parent)
    : base_type(parent)
{}

QnAbstractStatisticsModule::~QnAbstractStatisticsModule()
{}

QnMetricsHash QnAbstractStatisticsModule::metrics() const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual function called!");
    return QnMetricsHash();
}

