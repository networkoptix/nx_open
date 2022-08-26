// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_stream_synchronizer.h"

#include <camera/cam_display.h>
#include <camera/client_video_camera.h>
#include <camera/resource_display.h>
#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/counter.h>
#include <nx/utils/datetime.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/window_context.h>
#include <plugins/resource/archive/syncplay_wrapper.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/checked_cast.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnStreamSynchronizationState, (json), (isSyncOn)(timeUs)(speed))

using namespace nx::vms::client::desktop;

QnStreamSynchronizationState::QnStreamSynchronizationState():
    QnStreamSynchronizationState(false, AV_NOPTS_VALUE, 0.0)
{
}

QnStreamSynchronizationState::QnStreamSynchronizationState(bool isSyncOn, qint64 timeUs, qreal speed):
    isSyncOn(isSyncOn),
    timeUs(timeUs),
    speed(speed)
{
}

QnStreamSynchronizationState QnStreamSynchronizationState::live()
{
    return QnStreamSynchronizationState(true, DATETIME_NOW, 1.0);
}

QnWorkbenchStreamSynchronizer::QnWorkbenchStreamSynchronizer(
    WindowContext* windowContext,
    QObject* parent)
    :
    QObject(parent),
    WindowContextAware(windowContext),
    m_syncPlay(new QnArchiveSyncPlayWrapper(this)),
    m_watcher(windowContext->resourceWidgetRenderWatcher())
{
    NX_CRITICAL(m_watcher, "Initialization order failure");

    /* Connect to display. */
    connect(display(),
        &QnWorkbenchDisplay::widgetAdded,
        this,
        &QnWorkbenchStreamSynchronizer::at_display_widgetAdded);
    connect(display(),
        &QnWorkbenchDisplay::widgetAboutToBeRemoved,
        this,
        &QnWorkbenchStreamSynchronizer::at_display_widgetAboutToBeRemoved);
    connect(workbench(),
        &QnWorkbench::currentLayoutChanged,
        this,
        &QnWorkbenchStreamSynchronizer::at_workbench_currentLayoutChanged);

    /* Prepare counter. */
    m_counter = new nx::utils::CounterWithSignal(1, this);
    connect(this, &QObject::destroyed, m_counter, &nx::utils::CounterWithSignal::decrement);
    connect(m_counter,
        &nx::utils::CounterWithSignal::reachedZero,
        m_syncPlay,
        &QnArchiveSyncPlayWrapper::deleteLater);
    connect(m_counter,
        &nx::utils::CounterWithSignal::reachedZero,
        m_counter,
        &nx::utils::CounterWithSignal::deleteLater);

    /* Prepare render watcher instance. */
    connect(m_watcher,
        &QnWorkbenchRenderWatcher::displayChanged,
        this,
        &QnWorkbenchStreamSynchronizer::at_renderWatcher_displayChanged);

    /* Run handlers for all widgets already on display. */
    foreach (QnResourceWidget* widget, display()->widgets())
        at_display_widgetAdded(widget);
}

void QnWorkbenchStreamSynchronizer::stop() {
    if(!m_syncPlay->isEnabled())
        return;

    m_syncPlay->disableSync();

    emit runningChanged();
}

void QnWorkbenchStreamSynchronizer::start(qint64 timeUSec, float speed) {
    bool wasRunning = isRunning();

    m_syncPlay->enableSync(timeUSec, speed);

    if(!wasRunning)
        emit runningChanged();
}

bool QnWorkbenchStreamSynchronizer::isRunning() const {
    return m_syncPlay->isEnabled();
}

bool QnWorkbenchStreamSynchronizer::isEffective() const {
    return m_widgetCount > 0;
}

QnStreamSynchronizationState QnWorkbenchStreamSynchronizer::state() const
{
    QnStreamSynchronizationState result;

    result.isSyncOn = m_syncPlay->isEnabled();
    if (result.isSyncOn)
    {
        result.speed = m_syncPlay->getSpeed();
        result.timeUs = m_syncPlay->getCurrentTime();
    }

    return result;
}

void QnWorkbenchStreamSynchronizer::setState(const QnStreamSynchronizationState &state)
{
    if (state.isSyncOn)
        start(state.timeUs, state.speed);
    else
        stop();
}

void QnWorkbenchStreamSynchronizer::setState(QnResourceWidget* widget, bool useWidgetPausedState)
{
    QnStreamSynchronizationState state;
    if (QnMediaResourceWidget* mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
    {
        const auto display = mediaWidget->display();
        state.isSyncOn = true;
        state.timeUs = display->currentTimeUSec();
        state.speed = useWidgetPausedState && display->isPaused()
            ? 0.0
            : display->camDisplay()->getSpeed();
    }

    setState(state);
}

void QnWorkbenchStreamSynchronizer::at_display_widgetAdded(QnResourceWidget *widget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    if(!mediaWidget)
        return;

    connect(mediaWidget->resource()->toResource(), SIGNAL(flagsChanged(const QnResourcePtr &)), this, SLOT(at_resource_flagsChanged(const QnResourcePtr &)));

    if(!mediaWidget->resource()->toResource()->hasFlags(Qn::sync)) {
        m_queuedWidgets.insert(mediaWidget);
        return;
    }

    if(mediaWidget->display()->archiveReader() == nullptr)
        return;

    QnClientVideoCamera *camera = mediaWidget->display()->camera();
    m_syncPlay->addArchiveReader(mediaWidget->display()->archiveReader(), camera->getCamDisplay());
    camera->getCamDisplay()->setExternalTimeSource(m_syncPlay);

    m_counter->increment();
    connect(mediaWidget->display()->archiveReader(), SIGNAL(destroyed()), m_counter, SLOT(decrement()));

    m_widgetCount++;
    if(m_widgetCount == 1)
        emit effectiveChanged();
}

void QnWorkbenchStreamSynchronizer::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    if(!mediaWidget)
        return;

    disconnect(mediaWidget->resource()->toResource(), nullptr, this, nullptr);

    m_queuedWidgets.remove(mediaWidget);

    if(!mediaWidget->resource()->toResource()->hasFlags(Qn::sync))
        return;

    if(mediaWidget->display()->archiveReader() == nullptr)
        return;

    m_syncPlay->removeArchiveReader(mediaWidget->display()->archiveReader());

    m_widgetCount--;
    if(m_widgetCount == 0)
        emit effectiveChanged();
}

void QnWorkbenchStreamSynchronizer::at_renderWatcher_displayChanged(
    const QnResourceDisplayPtr& display)
{
    m_syncPlay->onConsumerBlocksReader(display->dataProvider(), !m_watcher->isDisplaying(display));
}

void QnWorkbenchStreamSynchronizer::at_workbench_currentLayoutChanged()
{
    QnTimePeriod period = workbench()->currentLayout()->resource()
        ? workbench()->currentLayout()->resource()->localRange()
        : QnTimePeriod();
    m_syncPlay->setLiveModeEnabled(period.isEmpty());
}

void QnWorkbenchStreamSynchronizer::at_resource_flagsChanged(const QnResourcePtr &resource) {
    if(!(resource->flags() & Qn::sync))
        return; // TODO: #sivanov Implement reverse handling?

    foreach(QnMediaResourceWidget *widget, m_queuedWidgets) {
        if(widget->resource()->toResourcePtr() == resource) {
            m_queuedWidgets.remove(widget);

            m_widgetCount++;
            if(m_widgetCount == 1)
                emit effectiveChanged();
        }
    }
}
