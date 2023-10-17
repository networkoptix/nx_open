// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <statistics/base/time_duration_metric.h>
#include <ui/workbench/workbench_context_aware.h>

class QnResourceWidget;
class QnMediaResourceWidget;

class MotionSearchDurationMetric:
    public QObject,
    public QnTimeDurationMetric,
    public QnWorkbenchContextAware
{
    typedef QObject base_type;

public:
    MotionSearchDurationMetric(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~MotionSearchDurationMetric();

private:
    typedef QPointer<QnMediaResourceWidget> QnMediaResourceWidgetPtr;

    void addWidget(QnResourceWidget* resourceWidget);

    void removeWidget(QnResourceWidget* resourceWidget);

    void updateCounterState();

    bool isFromCurrentLayout(const QnMediaResourceWidgetPtr& widget);

private:
    typedef std::set<QnMediaResourceWidgetPtr> WidgetsSet;
    WidgetsSet m_widgets;
};
