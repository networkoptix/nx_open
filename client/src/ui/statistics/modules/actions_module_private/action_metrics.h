
#pragma once

#include <ui/actions/actions.h>
#include <ui/statistics/modules/actions_module_private/abstract_action_metric.h>

class QnActionManager;

class ActionTriggeredCountMetric : public QObject
    , public AbstractActionMetric
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