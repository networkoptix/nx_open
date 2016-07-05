
#pragma once


#include <statistics/base/base_fwd.h>
#include <statistics/base/statistics_values_provider.h>

class QnAction;
class QnActionManager;
class TimeDurationMetric;

class AbstractActionsMetrics : public QObject
    , public QnStatisticsValuesProvider
{
    Q_OBJECT

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
    Q_OBJECT

    typedef AbstractActionsMetrics base_type;

public:
    ActionsTriggeredCountMetrics(QnActionManager *actionManager);

    QnStatisticValuesHash values() const override;

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
    Q_OBJECT

    typedef AbstractActionsMetrics base_type;

public:
    ActionCheckedTimeMetric(QnActionManager *actionManager);

    virtual ~ActionCheckedTimeMetric();

    QnStatisticValuesHash values() const override;

    void reset() override;

protected:
    void addActionMetric(QnAction *action) override;

private:
    QnMetricsContainerPtr m_metrics;
};
