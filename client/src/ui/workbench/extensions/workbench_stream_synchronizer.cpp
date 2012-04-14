#include "workbench_stream_synchronizer.h"
#include <utils/common/warnings.h>
#include <utils/common/counter.h>
#include <camera/resource_display.h>
#include <camera/camdisplay.h>
#include <camera/camera.h>
#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource_widget.h>
#include <plugins/resources/archive/syncplay_wrapper.h>
#include "workbench_render_watcher.h"

QnWorkbenchStreamSynchronizer::QnWorkbenchStreamSynchronizer(QnWorkbenchDisplay *display, QnWorkbenchRenderWatcher *renderWatcher, QObject *parent):
    QObject(parent),
    m_syncPlay(NULL),
    m_display(display),
    m_counter(NULL)
{
    if(display == NULL) {
        qnNullWarning(display);
        return;
    }

    /* Prepare syncplay. */
    m_syncPlay = new QnArchiveSyncPlayWrapper();

    /* Connect to display. */
    connect(display,    SIGNAL(widgetAdded(QnResourceWidget *)),              this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display,    SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),   this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    
    /* Prepare counter. */
    m_counter = new QnCounter(1);
    connect(this,                       SIGNAL(destroyed()),    m_counter,      SLOT(decrement()));
    connect(m_counter,                  SIGNAL(reachedZero()),  m_syncPlay,     SLOT(deleteLater()));
    connect(m_counter,                  SIGNAL(reachedZero()),  m_counter,      SLOT(deleteLater()));

    /* Prepare render watcher. */
    connect(renderWatcher, SIGNAL(displayingStateChanged(QnAbstractRenderer *, bool)), this, SLOT(at_renderWatcher_displayingStateChanged(QnAbstractRenderer *, bool)));
}

void QnWorkbenchStreamSynchronizer::setEnabled(bool enabled) {
    if(m_syncPlay->isEnabled() == enabled)
        return;

    m_syncPlay->setEnabled(enabled);
}

bool QnWorkbenchStreamSynchronizer::isEnabled() const {
    return m_syncPlay->isEnabled();
}

void QnWorkbenchStreamSynchronizer::at_display_widgetAdded(QnResourceWidget *widget) {
    if(!widget->resource()->checkFlags(QnResource::live_cam))
        return;

    if(widget->display()->archiveReader() == NULL) 
        return;
    
    CLVideoCamera *camera = widget->display()->camera();
    m_syncPlay->addArchiveReader(widget->display()->archiveReader(), camera->getCamDisplay());
    camera->setExternalTimeSource(m_syncPlay);
    camera->getCamDisplay()->setExternalTimeSource(m_syncPlay);

    m_counter->increment();
    connect(widget->display()->archiveReader(), SIGNAL(destroyed()), m_counter, SLOT(decrement()));
}

void QnWorkbenchStreamSynchronizer::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(!widget->resource()->checkFlags(QnResource::live_cam))
        return;

    if(widget->display()->archiveReader() == NULL) 
        return;

    m_syncPlay->removeArchiveReader(widget->display()->archiveReader());
}

void QnWorkbenchStreamSynchronizer::at_renderWatcher_displayingStateChanged(QnAbstractRenderer *renderer, bool displaying) {
    if(m_display.isNull())
        return;

    QnResourceWidget *widget = m_display.data()->widget(renderer);
    if(widget == NULL)
        return;

    CLVideoCamera* camera = widget->display()->camera();
    if(camera == NULL)
        return;

    m_syncPlay->onConsumerBlocksReader(widget->display()->dataProvider(), !displaying);
}

