
#include "app_active_time_metric.h"


AppActiveTimeMetric::AppActiveTimeMetric(QObject *parent)
    : base_type(parent)
    , ActiveTimeMetric()
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

    activateCounter(isActiveApplicationEvent);
    return false;
}

