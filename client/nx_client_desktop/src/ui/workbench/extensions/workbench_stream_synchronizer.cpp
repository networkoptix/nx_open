#include "workbench_stream_synchronizer.h"

#include <utils/common/warnings.h>
#include <nx/utils/counter.h>
#include <utils/common/checked_cast.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/security_cam_resource.h>

#include <plugins/resource/archive/syncplay_wrapper.h>

#include <camera/resource_display.h>
#include <camera/client_video_camera.h>
#include <camera/cam_display.h>

#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/watchers/workbench_render_watcher.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/datetime.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnStreamSynchronizationState, (json), (started)(time)(speed))

QnStreamSynchronizationState::QnStreamSynchronizationState():
    QnStreamSynchronizationState(false, AV_NOPTS_VALUE, 0.0)
{
}

QnStreamSynchronizationState::QnStreamSynchronizationState(bool started, qint64 time, qreal speed):
    started(started),
    time(time),
    speed(speed)
{
}

QnStreamSynchronizationState QnStreamSynchronizationState::live()
{
    return QnStreamSynchronizationState(true, DATETIME_NOW, 1.0);
}


QnWorkbenchStreamSynchronizer::QnWorkbenchStreamSynchronizer(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_widgetCount(0),
    m_counter(NULL),
    m_syncPlay(new QnArchiveSyncPlayWrapper(this)),
    m_watcher(context()->instance<QnWorkbenchRenderWatcher>())
{
    /* Connect to display. */
    connect(display(),                  &QnWorkbenchDisplay::widgetAdded,               this,       &QnWorkbenchStreamSynchronizer::at_display_widgetAdded);
    connect(display(),                  &QnWorkbenchDisplay::widgetAboutToBeRemoved,    this,       &QnWorkbenchStreamSynchronizer::at_display_widgetAboutToBeRemoved);
    connect(workbench(),                &QnWorkbench::currentLayoutChanged,             this,       &QnWorkbenchStreamSynchronizer::at_workbench_currentLayoutChanged);

    /* Prepare counter. */
    m_counter = new nx::utils::Counter(1, this);
    connect(this,                       &QObject::destroyed,                            m_counter,  &nx::utils::Counter::decrement);
    connect(m_counter,                  &nx::utils::Counter::reachedZero,                        m_syncPlay, &QnArchiveSyncPlayWrapper::deleteLater);
    connect(m_counter,                  &nx::utils::Counter::reachedZero,                        m_counter,  &nx::utils::Counter::deleteLater);

    /* Prepare render watcher instance. */
    connect(m_watcher,                  &QnWorkbenchRenderWatcher::displayChanged,      this,       &QnWorkbenchStreamSynchronizer::at_renderWatcher_displayChanged);

    /* Run handlers for all widgets already on display. */
    foreach(QnResourceWidget *widget, display()->widgets())
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

QnStreamSynchronizationState QnWorkbenchStreamSynchronizer::state() const {
    QnStreamSynchronizationState result;

    result.started = m_syncPlay->isEnabled();
    if(result.started) {
        result.speed = m_syncPlay->getSpeed();
        result.time = m_syncPlay->getCurrentTime();
    }

    return result;
}

void QnWorkbenchStreamSynchronizer::setState(const QnStreamSynchronizationState &state) {
    if(state.started) {
        start(state.time, state.speed);
    } else {
        stop();
    }
}

void QnWorkbenchStreamSynchronizer::setState(QnResourceWidget *widget) {
    QnStreamSynchronizationState state;

    if (QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget)) {
        state.started = true;
        state.time = mediaWidget->display()->currentTimeUSec();
        state.speed = mediaWidget->display()->camDisplay()->getSpeed();
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

    if(mediaWidget->display()->archiveReader() == NULL)
        return;

    QnClientVideoCamera *camera = mediaWidget->display()->camera();
    m_syncPlay->addArchiveReader(mediaWidget->display()->archiveReader(), camera->getCamDisplay());

    if (!(widget->options() & QnResourceWidget::SyncPlayForbidden))
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

    disconnect(mediaWidget->resource()->toResource(), NULL, this, NULL);

    m_queuedWidgets.remove(mediaWidget);

    if(!mediaWidget->resource()->toResource()->hasFlags(Qn::sync))
        return;

    if(mediaWidget->display()->archiveReader() == NULL)
        return;

    m_syncPlay->removeArchiveReader(mediaWidget->display()->archiveReader());
    /* //Unsafe in the common scenario: closing camera on the scene while we are in archive. CamDisplay does not use mutexes to guard external time source.
    QnClientVideoCamera *camera = mediaWidget->display()->camera();
    if (camera) {
        camera->setExternalTimeSource(nullptr);
        camera->getCamDisplay()->setExternalTimeSource(nullptr); // TODO: #Elric two setExternalTimeSource calls, WTF?
    }
    */

    m_widgetCount--;
    if(m_widgetCount == 0)
        emit effectiveChanged();
}

void QnWorkbenchStreamSynchronizer::at_renderWatcher_displayChanged(
    const QnResourceDisplayPtr& display)
{
    m_syncPlay->onConsumerBlocksReader(display->dataProvider(), !m_watcher->isDisplaying(display));
}

void QnWorkbenchStreamSynchronizer::at_workbench_currentLayoutChanged() {
    QnTimePeriod period = workbench()->currentLayout()->resource() ? workbench()->currentLayout()->resource()->getLocalRange() : QnTimePeriod();
    m_syncPlay->setLiveModeEnabled(period.isEmpty());
}

void QnWorkbenchStreamSynchronizer::at_resource_flagsChanged(const QnResourcePtr &resource) {
    if(!(resource->flags() & Qn::sync))
        return; // TODO: #Elric implement reverse handling?

    foreach(QnMediaResourceWidget *widget, m_queuedWidgets) {
        if(widget->resource()->toResourcePtr() == resource) {
            m_queuedWidgets.remove(widget);

            m_widgetCount++;
            if(m_widgetCount == 1)
                emit effectiveChanged();
        }
    }
}

