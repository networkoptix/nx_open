// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <statistics/base/time_duration_metric.h>

class QnResourceWidget;
class QnMediaResourceWidget;
class QnWorkbenchContext;

class MotionSearchDurationMetric : public QObject
    , public QnTimeDurationMetric
{
    typedef QObject base_type;

public:
    MotionSearchDurationMetric(QnWorkbenchContext *context);

    virtual ~MotionSearchDurationMetric();

private:
    typedef QPointer<QnMediaResourceWidget> QnMediaResourceWidgetPtr;

    void addWidget(QnResourceWidget *resourceWidget);

    void removeWidget(QnResourceWidget *resourceWidget);

    void updateCounterState();

    bool isFromCurrentLayout(const QnMediaResourceWidgetPtr &widget);

private:
    typedef QPointer<QnWorkbenchContext> QnWorkbenchContextPtr;
    typedef std::set<QnMediaResourceWidgetPtr> WidgetsSet;

    QnWorkbenchContextPtr m_context;
    WidgetsSet m_widgets;
};
