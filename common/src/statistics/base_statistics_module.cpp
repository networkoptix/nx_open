
#include "base_statistics_module.h"

QnBaseStatisticsModule::QnBaseStatisticsModule(QObject *parent)
    : base_type(parent)
{}

QnBaseStatisticsModule::~QnBaseStatisticsModule()
{}

QnMetricsHash QnBaseStatisticsModule::metrics() const
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Pure virtual function called!");
    return QnMetricsHash();
}

