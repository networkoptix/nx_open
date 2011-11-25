#include "sync_play_mixin.h"
#include <utils/common/warnings.h>
#include <camera/resource_display.h>
#include <camera/camdisplay.h>
#include <camera/camera.h>
#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource_widget.h>
#include <plugins/resources/archive/syncplay_wrapper.h>

QnSyncPlayMixin::QnSyncPlayMixin(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    m_syncPlay(NULL)
{
    if(display == NULL) {
        qnNullWarning(display);
        return;
    }

    m_syncPlay = new QnArchiveSyncPlayWrapper();
    m_syncPlay->setParent(this);

    connect(display,    SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display,    SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
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

