
#pragma once

#include <ui/actions/actions.h>
#include <ui/statistics/modules/module_private/actions/abstract_action_metric.h>
#include <ui/statistics/modules/module_private/active_time_metric.h>

class QnActionManager;

class ActionTriggeredCountMetric : public QObject
    , public AbstractActionMetric
{
    typedef QObject base_type;

public:
    static const QString kPostfix;

    ActionTriggeredCountMetric(QnActionManager *actionManager
        , QnActions::IDType id);

    QnMetricsHash metrics() const override;

    void reset() override;

private:
    void onTriggered();

private:
    typedef QHash<QString, int> TiggeredCountByParamsHash;
    TiggeredCountByParamsHash m_values;
};

//

class ActionCheckedTimeMetric : public QObject
    , public ActiveTimeMetric
{
    typedef QObject base_type;

public:
    ActionCheckedTimeMetric(QnActionManager *actionManager
        , QnActions::IDType id);

    virtual ~ActionCheckedTimeMetric();

private:
    void updateCounter(bool activate);
};