#include "workbench_media_widget_watcher.h"

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/graphics/items/resource/resource_widget.h>

QnWorkbenchMediaWidgetWatcher::QnWorkbenchMediaWidgetWatcher(QObject *parent) :
    QObject(parent),
    m_renderWatcher(context()->instance<QnWorkbenchRenderWatcher>())
{
    connect(m_renderWatcher, SIGNAL(displayingChanged(QnResourceWidget*)), this, SLOT(widgetDisplayingChanged(QnResourceWidget*)));
}

QnWorkbenchMediaWidgetWatcher::~QnWorkbenchMediaWidgetWatcher() {}

void QnWorkbenchMediaWidgetWatcher::adjustLayoutAspectRatio(QnWorkbenchLayout *layout) {
    m_watchedLayouts.insert(layout);
}

void QnWorkbenchMediaWidgetWatcher::widgetDisplayingChanged(QnResourceWidget *widget) {
    if (!m_renderWatcher->isDisplaying(widget))
        return;

    if (widget->hasAspectRatio())
        setLayoutAspectRatio(widget->item()->layout(), widget->aspectRatio());
    else
        connect(widget, SIGNAL(aspectRatioChanged()), this, SLOT(widgetAspectRatioChanged()));
}

void QnWorkbenchMediaWidgetWatcher::widgetAspectRatioChanged() {
    QnResourceWidget *widget = qobject_cast<QnResourceWidget*>(sender());
    if (!widget)
        return;

    setLayoutAspectRatio(widget->item()->layout(), widget->aspectRatio());
    disconnect(widget, SIGNAL(aspectRatioChanged()), this, SLOT(widgetAspectRatioChanged()));
}

void QnWorkbenchMediaWidgetWatcher::setLayoutAspectRatio(QnWorkbenchLayout *layout, qreal aspectRatio) {
    if (!m_watchedLayouts.contains(layout))
        return;

    layout->setCellAspectRatio(widget->aspectRatio());
    m_watchedLayouts.remove(layout);
}
