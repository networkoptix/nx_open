#include "workbench_layout_aspect_ratio_watcher.h"

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/graphics/items/resource/resource_widget.h>

namespace {
    const qreal normalAspectRatio = 4.0 / 3.0;
    const qreal wideAspectRatio = 16.0 / 9.0;
}

QnWorkbenchLayoutAspectRatioWatcher::QnWorkbenchLayoutAspectRatioWatcher(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_renderWatcher(context()->instance<QnWorkbenchRenderWatcher>()),
    m_watchedLayout(workbench()->currentLayout())
{
    connect(m_renderWatcher,    SIGNAL(widgetChanged(QnResourceWidget*)),       this,   SLOT(at_renderWatcher_widgetChanged(QnResourceWidget*)));
    connect(workbench(),        SIGNAL(currentLayoutAboutToBeChanged()),        this,   SLOT(at_workbench_currentLayoutAboutToBeChanged()));
    connect(workbench(),        SIGNAL(currentLayoutChanged()),                 this,   SLOT(at_workbench_currentLayoutChanged()));
}

QnWorkbenchLayoutAspectRatioWatcher::~QnWorkbenchLayoutAspectRatioWatcher() {}

void QnWorkbenchLayoutAspectRatioWatcher::at_renderWatcher_widgetChanged(QnResourceWidget *widget) {
    if (!m_renderWatcher->isDisplaying(widget))
        return;

    if (!m_watchedLayout)
        return;

    if (widget->hasAspectRatio()) {
        m_watchedLayout->setCellAspectRatio(widget->aspectRatio());
    } else {
        connect(widget, SIGNAL(aspectRatioChanged()), this, SLOT(at_resourceWidget_aspectRatioChanged()));
        connect(widget, SIGNAL(destroyed()),          this, SLOT(at_resourceWidget_destroyed()));
        m_watchedWidgets.insert(widget);
    }
}

void QnWorkbenchLayoutAspectRatioWatcher::at_resourceWidget_aspectRatioChanged() {
    if (m_watchedLayout->items().size() != 1)
        return;

    QnResourceWidget *widget = qobject_cast<QnResourceWidget*>(sender());
    if (!widget)
        return;

    disconnect(widget, SIGNAL(aspectRatioChanged()), this, SLOT(at_resourceWidget_aspectRatioChanged()));

    if (qAbs(widget->aspectRatio() - normalAspectRatio) < qAbs(widget->aspectRatio() - wideAspectRatio))
        m_watchedLayout->setCellAspectRatio(normalAspectRatio);
    else
        m_watchedLayout->setCellAspectRatio(wideAspectRatio);
}

void QnWorkbenchLayoutAspectRatioWatcher::at_resourceWidget_destroyed() {
    m_watchedWidgets.remove(static_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchLayoutAspectRatioWatcher::at_workbench_currentLayoutChanged() {
    watchCurrentLayout();
}

void QnWorkbenchLayoutAspectRatioWatcher::at_workbench_currentLayoutAboutToBeChanged() {
    unwatchCurrentLayout();
}

void QnWorkbenchLayoutAspectRatioWatcher::at_watchedLayout_cellAspectRatioChanged() {
    if (m_watchedLayout->hasCellAspectRatio())
        unwatchCurrentLayout();
}

void QnWorkbenchLayoutAspectRatioWatcher::watchCurrentLayout() {
    QnWorkbenchLayout *layout = workbench()->currentLayout();
    if (layout->hasCellAspectRatio())
        return;

    m_watchedLayout = layout;
    connect(layout, SIGNAL(cellAspectRatioChanged()), this, SLOT(at_watchedLayout_cellAspectRatioChanged()));
}

void QnWorkbenchLayoutAspectRatioWatcher::unwatchCurrentLayout() {
    if (m_watchedLayout) {
        disconnect(m_watchedLayout, 0, this, 0);
        m_watchedLayout = 0;
    }

    foreach (QnResourceWidget *widget, m_watchedWidgets)
        disconnect(widget, 0, this, 0);
    m_watchedWidgets.clear();
}
