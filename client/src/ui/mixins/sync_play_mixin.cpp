#include "sync_play_mixin.h"
#include <utils/common/warnings.h>
#include <camera/resource_display.h>
#include <camera/camdisplay.h>
#include <camera/camera.h>
#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource_widget.h>
#include <plugins/resources/archive/syncplay_wrapper.h>
#include "render_watch_mixin.h"

QnSyncPlayMixin::QnSyncPlayMixin(QnWorkbenchDisplay *display, QnRenderWatchMixin *renderWatcher, QObject *parent):
    QObject(parent),
    m_syncPlay(NULL),
    m_display(display)
{
    if(display == NULL) {
        qnNullWarning(display);
        return;
    }

    /* Prepare syncplay. */
    m_syncPlay = new QnArchiveSyncPlayWrapper();
    m_syncPlay->setParent(this);

    /* Connect to display. */
    connect(display,    SIGNAL(widgetAdded(QnResourceWidget *)),              this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display,    SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),   this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(display,    SIGNAL(playbackMaskChanged(const QnTimePeriodList&)), this,   SLOT(at_playback_mask_changed(const QnTimePeriodList&)));
    

    /* Prepare render watcher. */
    connect(renderWatcher, SIGNAL(displayingStateChanged(QnAbstractRenderer *, bool)), this, SLOT(at_renderWatcher_displayingStateChanged(QnAbstractRenderer *, bool)));
}

void QnSyncPlayMixin::at_display_widgetAdded(QnResourceWidget *widget) {
    if(!widget->resource()->checkFlag(QnResource::live_cam))
        return;

    if(widget->display()->archiveReader() == NULL) 
        return;
    
    CLVideoCamera *camera = widget->display()->camera();
    m_syncPlay->addArchiveReader(widget->display()->archiveReader(), camera->getCamCamDisplay());
    camera->setExternalTimeSource(m_syncPlay);
    camera->getCamCamDisplay()->setExternalTimeSource(m_syncPlay);
}

void QnSyncPlayMixin::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(!widget->resource()->checkFlag(QnResource::live_cam))
        return;

    if(widget->display()->archiveReader() == NULL) 
        return;

    m_syncPlay->removeArchiveReader(widget->display()->archiveReader());
}

void QnSyncPlayMixin::at_renderWatcher_displayingStateChanged(QnAbstractRenderer *renderer, bool displaying) {
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

void QnSyncPlayMixin::at_playback_mask_changed(const QnTimePeriodList& playbackMask)
{
    m_syncPlay->setPlaybackMask(playbackMask);
}
