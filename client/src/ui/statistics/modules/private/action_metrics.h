
#pragma once

#include <ui/statistics/modules/private/single_metrics_holder.h>

class QnAction;
class QnActionManager;
class TimeDurationMetric;

class AbstractActionsMetrics : public QObject
    , public AbstractMultimetric
{
    typedef QObject base_type;

public:
    AbstractActionsMetrics (QnActionManager *actionManager);

    virtual ~AbstractActionsMetrics();

protected:
    virtual void addActionMetric(QnAction *action) = 0;
};

//

class ActionsTriggeredCountMetrics : public AbstractActionsMetrics
{
    typedef AbstractActionsMetrics base_type;

public:
    ActionsTriggeredCountMetrics(QnActionManager *actionManager);

    QnMetricsHash metrics() const override;

    void reset() override;

protected:
    void addActionMetric(QnAction *action) override;

private:
    typedef QHash<QString, int> TiggeredCountByParamsHash;
    typedef QHash<int, TiggeredCountByParamsHash> OverallTriggeredCountHash;
    OverallTriggeredCountHash m_values;
};

//

class ActionCheckedTimeMetric : public AbstractActionsMetrics
{
    typedef AbstractActionsMetrics base_type;

public:
    ActionCheckedTimeMetric(QnActionManager *actionManager);

    virtual ~ActionCheckedTimeMetric();

    QnMetricsHash metrics() const override;

    void reset() override;

protected:
    void addActionMetric(QnAction *action) override;

private:
    typedef QSharedPointer<SingleMetricsHolder> SingleMetricsHolderPtr;

    SingleMetricsHolderPtr m_metrics;
};
