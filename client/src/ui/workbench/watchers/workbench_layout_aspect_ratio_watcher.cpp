#include "workbench_layout_aspect_ratio_watcher.h"

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/graphics/items/resource/resource_widget.h>

QnWorkbenchLayoutAspectRatioWatcher::QnWorkbenchLayoutAspectRatioWatcher(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_renderWatcher(context()->instance<QnWorkbenchRenderWatcher>()),
    m_watchedLayout(workbench()->currentLayout())
{
    connect(m_renderWatcher,    SIGNAL(displayingChanged(QnResourceWidget*)),   this,   SLOT(at_renderWatcher_displayingChanged(QnResourceWidget*)));
    connect(workbench(),        SIGNAL(currentLayoutAboutToBeChanged()),        this,   SLOT(at_workbench_currentLayoutAboutToBeChanged()));
    connect(workbench(),        SIGNAL(currentLayoutChanged()),                 this,   SLOT(at_workbench_currentLayoutChanged()));
}

QnWorkbenchLayoutAspectRatioWatcher::~QnWorkbenchLayoutAspectRatioWatcher() {}

void QnWorkbenchLayoutAspectRatioWatcher::at_renderWatcher_displayingChanged(QnResourceWidget *widget) {
    if (!m_renderWatcher->isDisplaying(widget))
        return;

    if (!m_watchedLayout)
        return;

    if (widget->hasAspectRatio()) {
        m_watchedLayout->setCellAspectRatio(widget->aspectRatio());
    } else {
        connect(widget, SIGNAL(aspectRatioChanged()), this, SLOT(at_resourceWidget_aspectRatioChanged()));
        m_watchedWidgets.insert(widget);
    }
}

void QnWorkbenchLayoutAspectRatioWatcher::at_resourceWidget_aspectRatioChanged() {
    if (m_watchedLayout->items().size() != 1)
        return;

    QnResourceWidget *widget = qobject_cast<QnResourceWidget*>(sender());
    if (!widget)
        return;

    m_watchedLayout->setCellAspectRatio(widget->aspectRatio());
    disconnect(widget, SIGNAL(aspectRatioChanged()), this, SLOT(at_resourceWidget_aspectRatioChanged()));
}

void QnWorkbenchLayoutAspectRatioWatcher::at_workbench_currentLayoutChanged() {
    watchCurrentLayout();
}

void QnWorkbenchLayoutAspectRatioWatcher::at_workbench_currentLayoutAboutToBeChanged() {
    unwatchCurrentLayout();
}

void QnWorkbenchLayoutAspectRatioWatcher::at_watchedLayout_cellAspectRatioChanged() {
    if (m_watchedLayout->cellAspectRatio() > 0)
        unwatchCurrentLayout();
}

void QnWorkbenchLayoutAspectRatioWatcher::watchCurrentLayout() {
    QnWorkbenchLayout *layout = workbench()->currentLayout();
    if (layout->cellAspectRatio() > 0)
        return;

    m_watchedLayout = layout;
    connect(layout, SIGNAL(cellAspectRatioChanged()), this, SLOT(at_watchedLayout_cellAspectRatioChanged()));
}

void QnWorkbenchLayoutAspectRatioWatcher::unwatchCurrentLayout() {
    disconnect(m_watchedLayout, 0, this, 0);
    m_watchedLayout = 0;

    foreach (QnResourceWidget *widget, m_watchedWidgets)
        disconnect(widget, 0, this, 0);
    m_watchedWidgets.clear();
}
