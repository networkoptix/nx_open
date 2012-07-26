#include "workbench_stream_synchronizer.h"

#include <utils/common/warnings.h>
#include <utils/common/counter.h>
#include <utils/common/checked_cast.h>

#include <core/resource/resource.h>
#include <core/resource/security_cam_resource.h>

#include <plugins/resources/archive/syncplay_wrapper.h>

#include <camera/resource_display.h>
#include <camera/camdisplay.h>
#include <camera/camera.h>

#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include "workbench_render_watcher.h"

QnWorkbenchStreamSynchronizer::QnWorkbenchStreamSynchronizer(QnWorkbenchDisplay *display, QnWorkbenchRenderWatcher *renderWatcher, QObject *parent):
    QObject(parent),
    m_widgetCount(0),
    m_syncPlay(NULL),
    m_display(display),
    m_counter(NULL)
{
    if(display == NULL) {
        qnNullWarning(display);
        return;
    }

    /* Prepare syncplay. */
    m_syncPlay = new QnArchiveSyncPlayWrapper(); // TODO: QnArchiveSyncPlayWrapper destructor doesn't get called, investigate.

    /* Connect to display. */
    connect(display,    SIGNAL(widgetAdded(QnResourceWidget *)),              this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display,    SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),   this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    
    /* Prepare counter. */
    m_counter = new QnCounter(1); // TODO: this one also doesn't get destroyed.
    connect(this,                       SIGNAL(destroyed()),    m_counter,      SLOT(decrement()));
    connect(m_counter,                  SIGNAL(reachedZero()),  m_syncPlay,     SLOT(deleteLater()));
    connect(m_counter,                  SIGNAL(reachedZero()),  m_counter,      SLOT(deleteLater()));

    /* Prepare render watcher. */
    connect(renderWatcher, SIGNAL(displayingStateChanged(QnAbstractRenderer *, bool)), this, SLOT(at_renderWatcher_displayingStateChanged(QnAbstractRenderer *, bool)));

    /* Run handlers for all widgets already on display. */
    foreach(QnResourceWidget *widget, display->widgets())
        at_display_widgetAdded(widget);
}

void QnWorkbenchStreamSynchronizer::disableSync() {
    if(m_syncPlay->isEnabled())
        m_syncPlay->disableSync();
}

void QnWorkbenchStreamSynchronizer::enableSync(qint64 currentTime, float speed) 
{
    if(!m_syncPlay->isEnabled())
        m_syncPlay->enableSync(currentTime, speed);
}

bool QnWorkbenchStreamSynchronizer::isEnabled() const {
    return m_syncPlay->isEnabled();
}

bool QnWorkbenchStreamSynchronizer::isEffective() const {
    return m_widgetCount > 0;
}

void QnWorkbenchStreamSynchronizer::at_display_widgetAdded(QnResourceWidget *widget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    if(!mediaWidget)
        return;

    connect(mediaWidget->resource().data(), SIGNAL(flagsChanged()), this, SLOT(at_resource_flagsChanged()));

    if(!mediaWidget->resource()->checkFlags(QnResource::utc)) {
        m_queuedWidgets.insert(mediaWidget);
        return;
    }

    if(mediaWidget->display()->archiveReader() == NULL) 
        return;
    
    CLVideoCamera *camera = mediaWidget->display()->camera();
    m_syncPlay->addArchiveReader(mediaWidget->display()->archiveReader(), camera->getCamDisplay());
    camera->setExternalTimeSource(m_syncPlay);
    camera->getCamDisplay()->setExternalTimeSource(m_syncPlay);

    m_counter->increment();
    connect(mediaWidget->display()->archiveReader(), SIGNAL(destroyed()), m_counter, SLOT(decrement()));

    m_widgetCount++;
    if(m_widgetCount == 1) 
    {
        if(!mediaWidget->resource().dynamicCast<QnSecurityCamResource>())
            mediaWidget->display()->archiveReader()->jumpTo(0, 0); // change current position from live to left edge if it is not camera

        emit effectiveChanged();
    }
}

void QnWorkbenchStreamSynchronizer::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    if(!mediaWidget)
        return;

    disconnect(mediaWidget->resource().data(), NULL, this, NULL);

    m_queuedWidgets.remove(mediaWidget);

    if(!mediaWidget->resource()->checkFlags(QnResource::utc))
        return;

    if(mediaWidget->display()->archiveReader() == NULL) 
        return;

    m_syncPlay->removeArchiveReader(mediaWidget->display()->archiveReader());

    m_widgetCount--;
    if(m_widgetCount == 0)
        emit effectiveChanged();
}

void QnWorkbenchStreamSynchronizer::at_renderWatcher_displayingStateChanged(QnAbstractRenderer *renderer, bool displaying) {
    if(m_display.isNull())
        return;

    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(m_display.data()->widget(renderer));
    if(mediaWidget == NULL)
        return;

    m_syncPlay->onConsumerBlocksReader(mediaWidget->display()->dataProvider(), !displaying);
}

void QnWorkbenchStreamSynchronizer::at_resource_flagsChanged() {
    if(!sender())
        return;

    at_resource_flagsChanged(checked_cast<QnResource *>(sender())->toSharedPointer());
}

void QnWorkbenchStreamSynchronizer::at_resource_flagsChanged(const QnResourcePtr &resource) {
    if(!(resource->flags() & QnResource::utc))
        return; // TODO: implement reverse handling?

    foreach(QnMediaResourceWidget *widget, m_queuedWidgets) {
        if(widget->resource() == resource) {
            m_queuedWidgets.remove(widget);

            m_widgetCount++;
            if(m_widgetCount == 1)
                emit effectiveChanged();
        }
    }
}

