
#pragma once

#include <ui/actions/actions.h>
#include <ui/statistics/private/statistics_metric.h>

class QnActionManager;

class ActionTriggeredCountMetric : public QObject
    , public QnStatisticsMetric
{
public:
    static const QString kPostfix;

    ActionTriggeredCountMetric(QnActionManager *actionManager
        , Qn::ActionId id);

private:
    QnMetricsHash metrics() const override;

    void reset() override;
private:
    typedef QHash<QString, int> TiggeredCountByParamsHash;
    TiggeredCountByParamsHash m_values;
};