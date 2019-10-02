#pragma once

#include <statistics/abstract_statistics_module.h>

class QnGenericStatisticsModule: public QnAbstractStatisticsModule
{
    using base_type = QnAbstractStatisticsModule;
public:
    QnGenericStatisticsModule(QObject* parent = nullptr);

    virtual QnStatisticValuesHash values() const override;

    virtual void reset() override;
};
