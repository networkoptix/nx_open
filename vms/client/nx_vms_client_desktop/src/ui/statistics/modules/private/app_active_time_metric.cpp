// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "app_active_time_metric.h"

#include <QtWidgets/QApplication>

AppActiveTimeMetric::AppActiveTimeMetric(QObject *parent)
    : base_type(parent)
    , QnTimeDurationMetric()
{
    qApp->installEventFilter(this);
}

AppActiveTimeMetric::~AppActiveTimeMetric()
{}


bool AppActiveTimeMetric::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != qApp)
        return false;

    const bool isActiveApplicationEvent =
        (event->type() == QEvent::ApplicationActivate);

    if (!isActiveApplicationEvent && (event->type() != QEvent::ApplicationDeactivate))
        return false;

    setCounterActive(isActiveApplicationEvent);
    return false;
}

