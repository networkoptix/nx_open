#include "workbench_layout_aspect_ratio_watcher.h"

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/graphics/items/resource/resource_widget.h>

QnWorkbenchLayoutAspectRatioWatcher::QnWorkbenchLayoutAspectRatioWatcher(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_renderWatcher(context()->instance<QnWorkbenchRenderWatcher>())
{
    connect(m_renderWatcher, SIGNAL(displayingChanged(QnResourceWidget*)), this, SLOT(at_renderWatcher_displayingChanged(QnResourceWidget*)));
}

QnWorkbenchLayoutAspectRatioWatcher::~QnWorkbenchLayoutAspectRatioWatcher() {}

void QnWorkbenchLayoutAspectRatioWatcher::watchLayout(QnWorkbenchLayout *layout) {
    m_watchedLayouts.insert(layout);
    connect(layout,     SIGNAL(destroyed()),                    this,   SLOT(at_watchedLayout_destroyed()));
    connect(layout,     SIGNAL(itemRemoved(QnWorkbenchItem*)),  this,   SLOT(at_watchedLayout_itemRemoved()));
    connect(layout,     SIGNAL(itemAdded(QnWorkbenchItem*)),    this,   SLOT(at_watchedLayout_itemAdded()));
}

void QnWorkbenchLayoutAspectRatioWatcher::removeLayout(QnWorkbenchLayout *layout) {
    disconnect(layout, 0, this, 0);
    m_watchedLayouts.remove(layout);
}

QSet<QnWorkbenchLayout *> QnWorkbenchLayoutAspectRatioWatcher::watchedLayouts() const {
    return m_watchedLayouts;
}

void QnWorkbenchLayoutAspectRatioWatcher::at_renderWatcher_displayingChanged(QnResourceWidget *widget) {
    if (!m_renderWatcher->isDisplaying(widget))
        return;

    if (widget->hasAspectRatio())
        setLayoutAspectRatio(widget->item()->layout(), widget->aspectRatio());
    else
        connect(widget, SIGNAL(aspectRatioChanged()), this, SLOT(at_resourceWidget_aspectRatioChanged()));
}

void QnWorkbenchLayoutAspectRatioWatcher::at_resourceWidget_aspectRatioChanged() {
    QnResourceWidget *widget = qobject_cast<QnResourceWidget*>(sender());
    if (!widget)
        return;

    setLayoutAspectRatio(widget->item()->layout(), widget->aspectRatio());
    disconnect(widget, SIGNAL(aspectRatioChanged()), this, SLOT(at_resourceWidget_aspectRatioChanged()));
}

void QnWorkbenchLayoutAspectRatioWatcher::at_watchedLayout_destroyed() {
    if (QnWorkbenchLayout *layout = qobject_cast<QnWorkbenchLayout*>(sender()))
        m_watchedLayouts.remove(layout);
}

void QnWorkbenchLayoutAspectRatioWatcher::at_watchedLayout_itemAdded() {
    if (QnWorkbenchLayout *layout = qobject_cast<QnWorkbenchLayout*>(sender()))
        removeLayout(layout);
}

void QnWorkbenchLayoutAspectRatioWatcher::at_watchedLayout_itemRemoved() {
    if (QnWorkbenchLayout *layout = qobject_cast<QnWorkbenchLayout*>(sender()))
        removeLayout(layout);
}

void QnWorkbenchLayoutAspectRatioWatcher::setLayoutAspectRatio(QnWorkbenchLayout *layout, qreal aspectRatio) {
    if (!m_watchedLayouts.contains(layout))
        return;

    layout->setCellAspectRatio(aspectRatio);
    m_watchedLayouts.remove(layout);
}
