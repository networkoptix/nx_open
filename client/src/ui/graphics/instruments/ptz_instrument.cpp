#include "ptz_instrument.h"

#include <QtGui/QGraphicsSceneMouseEvent>

#include <utils/common/checked_cast.h>

#include <core/resource/camera_resource.h>
#include <core/resource/video_server_resource.h>
#include <core/resourcemanagment/resource_pool.h>

#include <api/video_server_connection.h>

#include <ui/graphics/items/resource/media_resource_widget.h>

PtzInstrument::PtzInstrument(QObject *parent): 
    base_type(Item, makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), parent) 
{}

PtzInstrument::~PtzInstrument() {
    ensureUninstalled();
}

bool PtzInstrument::registeredNotify(QGraphicsItem *item) {
    return base_type::registeredNotify(item) && dynamic_cast<QnMediaResourceWidget *>(item);
}

bool PtzInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    QnMediaResourceWidget *target = checked_cast<QnMediaResourceWidget *>(item);
    if(!(target->options() & QnResourceWidget::ControlPtz))
        return false;

    if(!target->rect().contains(event->pos()))
        return false; /* Ignore clicks on widget frame. */

    if(QnNetworkResourcePtr camera = target->resource().dynamicCast<QnNetworkResource>()) {
        if(QnVideoServerResourcePtr server = camera->resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnVideoServerResource>()) {
            m_target = target;
            m_camera = camera;
            m_serverSpeed = QVector3D();
            m_localSpeed = QVector3D();
            m_connection = server->apiConnection();
            
            dragProcessor()->mousePressEvent(item, event);
        }
    }

    event->accept();
    return false;
}

void PtzInstrument::startDragProcess(DragInfo *) {
    emit ptzProcessStarted(target());
}

void PtzInstrument::startDrag(DragInfo *) {
    emit ptzStarted(target());
}

void PtzInstrument::dragMove(DragInfo *info) {
    QPointF speed = cwiseDiv(info->mouseItemPos() - target()->rect().center(), target()->rect().size());
    m_localSpeed.setX(speed.x());
    m_localSpeed.setY(speed.y());

    if((m_localSpeed - m_serverSpeed).lengthSquared() > 0.1 * 0.1) {
        m_connection->asyncPtzMove(m_camera, m_localSpeed.x(), -m_localSpeed.y(), m_localSpeed.z(), this, SLOT(at_replyReceived(int, int)));
        m_serverSpeed = m_localSpeed;

        qDebug() << "PTZ set speed " << m_localSpeed;
    }
}

void PtzInstrument::finishDrag(DragInfo *) {
    emit ptzFinished(target());
}

void PtzInstrument::finishDragProcess(DragInfo *) {
    if(!m_serverSpeed.isNull()) {
        m_connection->asyncPtzStop(m_camera, this, SLOT(at_replyReceived(int, int)));
        m_serverSpeed = QVector3D();
    }

    m_localSpeed = QVector3D();
    m_camera = QnNetworkResourcePtr();
    m_connection = QnVideoServerConnectionPtr();

    emit ptzProcessFinished(target());
}

void PtzInstrument::at_replyReceived(int status, int handle) {
    Q_UNUSED(status);
    Q_UNUSED(handle);
}




