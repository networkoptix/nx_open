// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_stream_synchronizer.h"

#include <camera/cam_display.h>
#include <camera/client_video_camera.h>
#include <camera/resource_display.h>
#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>
#include <nx/utils/counter.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <plugins/resource/archive/syncplay_archive_delegate.h>
#include <plugins/resource/archive/syncplay_wrapper.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/watchers/workbench_render_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>

using namespace nx::vms::client::desktop;

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
        &Workbench::currentLayoutChanged,
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
    return !m_syncedWidgets.isEmpty();
}

StreamSynchronizationState QnWorkbenchStreamSynchronizer::state() const
{
    StreamSynchronizationState result;

    result.isSyncOn = m_syncPlay->isEnabled();
    if (result.isSyncOn)
    {
        result.speed = m_syncPlay->getSpeed();
        if (!m_syncedWidgets.empty())
            result.timeUs = m_syncPlay->getCurrentTime();
    }

    return result;
}

void QnWorkbenchStreamSynchronizer::setState(const StreamSynchronizationState &state)
{
    if (state.isSyncOn)
        start(state.timeUs, state.speed);
    else
        stop();
}

void QnWorkbenchStreamSynchronizer::setState(QnResourceWidget* widget, bool useWidgetPausedState)
{
    StreamSynchronizationState state;
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

    const auto resource = mediaWidget->resource()->toResource();

    connect(
        resource,
        &QnResource::flagsChanged,
        this,
        &QnWorkbenchStreamSynchronizer::at_resource_flagsChanged);

    handleWidget(mediaWidget);
}

void QnWorkbenchStreamSynchronizer::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    if(!mediaWidget)
        return;

    disconnect(mediaWidget->resource()->toResource(), nullptr, this, nullptr);

    m_queuedWidgets.remove(mediaWidget);
    if (m_syncedWidgets.contains(mediaWidget))
    {
        m_syncPlay->removeArchiveReader(mediaWidget->display()->archiveReader());
        m_syncedWidgets.remove(mediaWidget);
    }

    if(m_syncedWidgets.isEmpty())
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

void QnWorkbenchStreamSynchronizer::at_resource_flagsChanged(const QnResourcePtr& resource) {
    std::list<QnMediaResourceWidget*> affectedWidgets;
    const auto insertAffectedWidget =
        [&affectedWidgets, &resource] (QnMediaResourceWidget* widget)
        {
            if(widget->resource()->toResourcePtr() == resource)
                affectedWidgets.push_back(widget);
        };

    for (const auto& widget: m_syncedWidgets)
        insertAffectedWidget(widget);

    for (const auto& widget: m_queuedWidgets)
        insertAffectedWidget(widget);

    for (const auto& widget: affectedWidgets)
        handleWidget(widget);
}

void QnWorkbenchStreamSynchronizer::handleWidget(QnMediaResourceWidget* widget)
{
    NX_ASSERT(!(m_syncedWidgets.contains(widget) && m_queuedWidgets.contains(widget)));

    const auto resource = widget->resource()->toResource();
    const bool hasArchiveReader = widget->display()->archiveReader() != nullptr;
    const bool hasToBeSynced = hasArchiveReader && resource->hasFlags(Qn::sync);
    const bool isSyncedAlready = m_syncedWidgets.contains(widget);

    // This safety measure is not quite architecturally correct, but should be here until code
    // is cleaned up.
    const bool isSyncedActually = hasArchiveReader && dynamic_cast<QnSyncPlayArchiveDelegate*>(
        widget->display()->archiveReader()->getArchiveDelegate());

    if (hasToBeSynced && !isSyncedAlready)
    {
        NX_ASSERT(!isSyncedActually, "Syncplay wrapper already handles this camera");
    }

    if (hasToBeSynced && !isSyncedAlready && !isSyncedActually)
    {
        // New or queued widget.
        QnClientVideoCamera* camera = widget->display()->camera();
        m_syncPlay->addArchiveReader(widget->display()->archiveReader(), camera->getCamDisplay());
        camera->getCamDisplay()->setExternalTimeSource(m_syncPlay);

        m_counter->increment();
        connect(
            widget->display()->archiveReader(),
            &QnAbstractArchiveStreamReader::destroyed,
            m_counter,
            &nx::utils::CounterWithSignal::decrement);

        m_syncedWidgets.insert(widget);
        m_queuedWidgets.remove(widget);
    }
    else if (!hasToBeSynced)
    {
        if (isSyncedAlready)
        {
            NX_ASSERT(false, "Actually we should never be here. Widget is either syncable or not");

            // Stop sync.
            const auto archiveReader = widget->display()->archiveReader();
            if (NX_ASSERT(archiveReader))
            {
                m_syncPlay->removeArchiveReader(archiveReader);
                widget->display()->camera()->getCamDisplay()->setExternalTimeSource(nullptr);

                m_counter->decrement();
                archiveReader->disconnect(m_counter);
            }

            m_syncedWidgets.remove(widget);
        }

        m_queuedWidgets.insert(widget);
    }

    const auto syncedWidgetsCount = m_syncedWidgets.size();
    if (syncedWidgetsCount <= 1)
        emit effectiveChanged();
}
