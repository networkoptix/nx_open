// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/menu/action_fwd.h>
#include <statistics/base/base_fwd.h>
#include <statistics/base/statistics_values_provider.h>

class TimeDurationMetric;

class AbstractActionsMetrics : public QObject, public QnStatisticsValuesProvider
{
    Q_OBJECT

    typedef QObject base_type;

public:
    AbstractActionsMetrics(const nx::vms::client::desktop::menu::ManagerPtr& actionManager);

    virtual ~AbstractActionsMetrics();

protected:
    virtual void addActionMetric(nx::vms::client::desktop::menu::Action* action) = 0;
};

//

class ActionsTriggeredCountMetrics : public AbstractActionsMetrics
{
    Q_OBJECT

    typedef AbstractActionsMetrics base_type;

public:
    ActionsTriggeredCountMetrics(const nx::vms::client::desktop::menu::ManagerPtr& actionManager);

    QnStatisticValuesHash values() const override;

    void reset() override;

protected:
    void addActionMetric(nx::vms::client::desktop::menu::Action* action) override;

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
    ActionCheckedTimeMetric(const nx::vms::client::desktop::menu::ManagerPtr& actionManager);

    virtual ~ActionCheckedTimeMetric();

    QnStatisticValuesHash values() const override;

    void reset() override;

protected:
    void addActionMetric(nx::vms::client::desktop::menu::Action* action) override;

private:
    QnMetricsContainerPtr m_metrics;
};
