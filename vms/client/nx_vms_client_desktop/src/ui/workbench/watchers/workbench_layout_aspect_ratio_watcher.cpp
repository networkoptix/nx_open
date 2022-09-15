// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_aspect_ratio_watcher.h"

#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/aspect_ratio.h>

using namespace nx::vms::client::desktop;

QnWorkbenchLayoutAspectRatioWatcher::QnWorkbenchLayoutAspectRatioWatcher(
    WindowContext* windowContext,
    QObject* parent)
    :
    QObject(parent),
    WindowContextAware(windowContext),
    m_renderWatcher(windowContext->resourceWidgetRenderWatcher())
{
    NX_CRITICAL(m_renderWatcher, "Initialization order failure");
    connect(m_renderWatcher,
        &QnWorkbenchRenderWatcher::widgetChanged,
        this,
        &QnWorkbenchLayoutAspectRatioWatcher::at_renderWatcher_widgetChanged);
    connect(workbench(),
        &Workbench::currentLayoutAboutToBeChanged,
        this,
        &QnWorkbenchLayoutAspectRatioWatcher::at_workbench_currentLayoutAboutToBeChanged);
    connect(workbench(),
        &Workbench::currentLayoutChanged,
        this,
        &QnWorkbenchLayoutAspectRatioWatcher::at_workbench_currentLayoutChanged);
}

QnWorkbenchLayoutAspectRatioWatcher::~QnWorkbenchLayoutAspectRatioWatcher()
{
}

void QnWorkbenchLayoutAspectRatioWatcher::setAppropriateAspectRatio(QnResourceWidget* widget)
{
    if (widget->hasAspectRatio())
    {
        float aspectRatio = widget->visualAspectRatio();

        // Only current Widget geometry is relevant.
        QRect geometry = widget->item()->geometry();
        if (!geometry.isEmpty())
            aspectRatio /= nx::vms::client::core::Geometry::aspectRatio(geometry);

        if (!m_watchedLayout->flags().testFlag(QnLayoutFlag::FillViewport))
            aspectRatio = QnAspectRatio::closestStandardRatio(aspectRatio).toFloat();

        m_watchedLayout->resource()->setCellAspectRatio(aspectRatio);
    }
}

void QnWorkbenchLayoutAspectRatioWatcher::at_renderWatcher_widgetChanged(QnResourceWidget* widget)
{
    if (!m_renderWatcher->isDisplaying(widget))
        return;

    if (!m_watchedLayout || !m_watchedLayout->canAutoAdjustAspectRatio())
        return;

    setAppropriateAspectRatio(widget);

    if (m_monitoring || !widget->hasAspectRatio())
    {
        m_watchedWidgets.insert(widget);
        connect(widget,     &QnResourceWidget::aspectRatioChanged,  this,   &QnWorkbenchLayoutAspectRatioWatcher::at_resourceWidget_aspectRatioChanged);
        connect(widget,     &QnResourceWidget::destroyed,           this,   &QnWorkbenchLayoutAspectRatioWatcher::at_resourceWidget_destroyed);
    }
}

void QnWorkbenchLayoutAspectRatioWatcher::at_resourceWidget_aspectRatioChanged()
{
    if (!m_watchedLayout->canAutoAdjustAspectRatio())
        return;

    QnResourceWidget *widget = qobject_cast<QnResourceWidget*>(sender());
    if (!widget || !widget->hasAspectRatio())
        return;

    if (!m_monitoring)
    {
        disconnect(widget, &QnResourceWidget::aspectRatioChanged,
            this, &QnWorkbenchLayoutAspectRatioWatcher::at_resourceWidget_aspectRatioChanged);
    }

    setAppropriateAspectRatio(widget);
}

void QnWorkbenchLayoutAspectRatioWatcher::at_resourceWidget_destroyed()
{
    m_watchedWidgets.remove(static_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchLayoutAspectRatioWatcher::at_workbench_currentLayoutChanged()
{
    watchCurrentLayout();
}

void QnWorkbenchLayoutAspectRatioWatcher::at_workbench_currentLayoutAboutToBeChanged()
{
    unwatchCurrentLayout();
}

void QnWorkbenchLayoutAspectRatioWatcher::at_watchedLayout_cellAspectRatioChanged()
{
    if (m_watchedLayout->hasCellAspectRatio())
        unwatchCurrentLayout();
}

void QnWorkbenchLayoutAspectRatioWatcher::watchCurrentLayout()
{
    QnWorkbenchLayout *layout = workbench()->currentLayout();
    if (!m_monitoring && layout->hasCellAspectRatio())
        return;

    m_watchedLayout = layout;

    if (!m_monitoring)
        connect(layout, &QnWorkbenchLayout::cellAspectRatioChanged, this, &QnWorkbenchLayoutAspectRatioWatcher::at_watchedLayout_cellAspectRatioChanged);
}

void QnWorkbenchLayoutAspectRatioWatcher::unwatchCurrentLayout()
{
    if (m_watchedLayout)
    {
        m_watchedLayout->disconnect(this);
        m_watchedLayout = 0;
    }

    for (QnResourceWidget *widget: m_watchedWidgets)
        widget->QObject::disconnect(this);
    m_watchedWidgets.clear();
}
