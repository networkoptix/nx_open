#include "workbench_ptz_controller.h"

#include <utils/common/math.h>

#include <api/media_server_connection.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_ptz_cameras_watcher.h>

QnWorkbenchPtzController::QnWorkbenchPtzController(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    QnWorkbenchPtzCamerasWatcher *watcher = context()->instance<QnWorkbenchPtzCamerasWatcher>();
    connect(watcher, SIGNAL(ptzCameraAdded(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &)));
    connect(watcher, SIGNAL(ptzCameraRemoved(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &)));
    foreach(const QnVirtualCameraResourcePtr &camera, watcher->ptzCameras())
        at_ptzCameraWatcher_ptzCameraAdded(camera);


}

QnWorkbenchPtzController::~QnWorkbenchPtzController() {

}

QVector3D QnWorkbenchPtzController::position(const QnVirtualCameraResourcePtr &camera) {
    return QVector3D();
}

void QnWorkbenchPtzController::setPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &position) {

}

QVector3D QnWorkbenchPtzController::movement(const QnVirtualCameraResourcePtr &camera) {
    return QVector3D();
}

void QnWorkbenchPtzController::setMovement(const QnVirtualCameraResourcePtr &camera, const QVector3D &motion) {

}

void QnWorkbenchPtzController::sendGetPosition(const QnVirtualCameraResourcePtr &camera) {
    QnMediaServerResourcePtr server = resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();

    int handle = server->apiConnection()->asyncPtzGetPos(camera, this, SLOT(at_ptzGetPosition_replyReceived(int, qreal, qreal, qreal, int)));
    m_cameraByHandle[handle] = camera;

    PtzData &data = m_dataByCamera[camera];
    data.attemptCount[GetPositionRequest]++;
    data.handle[GetPositionRequest] = handle;
}

void QnWorkbenchPtzController::sendSetPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &position) {
    QnMediaServerResourcePtr server = resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();

    int handle = server->apiConnection()->asyncPtzMoveTo(camera, position.x(), position.y(), position.z(), this, SLOT(at_ptzSetPosition_replyReceived()));
    m_cameraByHandle[handle] = camera;

    PtzData &data = m_dataByCamera[camera];
    data.attemptCount[SetPositionRequest]++;
    data.handle[SetPositionRequest] = handle;
    data.pendingPosition = position;
}

void QnWorkbenchPtzController::sendSetMovement(const QnVirtualCameraResourcePtr &camera, const QVector3D &movement) {
    QnMediaServerResourcePtr server = resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();

    int handle;
    if(qFuzzyIsNull(movement)) {
        handle = server->apiConnection()->asyncPtzStop(camera, this, SLOT(at_ptzSetMovement_replyReceived(int, int)));
    } else {
        handle = server->apiConnection()->asyncPtzMove(camera, movement.x(), movement.y(), movement.z(), this, SLOT(at_ptzSetMovement_replyReceived(int, int)));
    }
    m_cameraByHandle[handle] = camera;

    PtzData &data = m_dataByCamera[camera];
    data.attemptCount[SetMovementRequest]++;
    data.handle[SetMovementRequest] = handle;
    data.pendingMovement = movement;
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchPtzController::at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera) {
    QnMediaServerResourcePtr server = resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();

    int handle;
    
    handle = server->apiConnection()->asyncPtzGetPos(camera, this, SLOT(at_ptzGetPosition_replyReceived(int, qreal, qreal, qreal, int)));
    m_cameraByHandle[handle] = camera;

    //handle = server->apiConnection()->asyncPtz

}

void QnWorkbenchPtzController::at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera) {

}

