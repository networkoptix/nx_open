#include "workbench_ptz_controller.h"

#include <utils/common/math.h>
#include <utils/common/checked_cast.h>

#include <api/media_server_connection.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_ptz_cameras_watcher.h>

namespace {
    template<class Key, class T>
    void qnRemoveValues(QHash<Key, T> &hash, const T &value) {
        for(typename QHash<Key, T>::iterator pos = hash.begin(); pos != hash.end();) {
            if(pos.value() == value) {
                pos = hash.erase(pos);
            } else {
                pos++;
            }
        }
    }

    const int maxAttempts = 5;

} // anonymous namespace

QnWorkbenchPtzController::PtzData::PtzData(): initialized(false) {
    qreal realNaN = std::numeric_limits<qreal>::quiet_NaN();
    QVector3D vectorNaN = QVector3D(realNaN, realNaN, realNaN);

    pendingPosition = knownPosition = pendingMovement = knownMovement = vectorNaN;

    for(int i = 0; i < RequestTypeCount; i++)
        attemptCount[i] = 0;
}


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
}

void QnWorkbenchPtzController::sendSetPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &position) {
    QnMediaServerResourcePtr server = resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();

    int handle = server->apiConnection()->asyncPtzMoveTo(camera, position.x(), position.y(), position.z(), this, SLOT(at_ptzSetPosition_replyReceived()));
    m_cameraByHandle[handle] = camera;
    m_requestByHandle[handle] = position;

    PtzData &data = m_dataByCamera[camera];
    data.attemptCount[SetPositionRequest]++;
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
    m_requestByHandle[handle] = movement;

    PtzData &data = m_dataByCamera[camera];
    data.attemptCount[SetMovementRequest]++;
    data.pendingMovement = movement;
}

void QnWorkbenchPtzController::tryInitialize(const QnVirtualCameraResourcePtr &camera) {
    PtzData &data = m_dataByCamera[camera];
    if(data.initialized)
        return;

    QnResource::Status status = camera->getStatus();
    if(status == QnResource::Offline || status == QnResource::Unauthorized)
        return;

    data.initialized = true;
    sendGetPosition(camera);
    sendSetMovement(camera, QVector3D());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchPtzController::at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera) {
    connect(camera.data(), SIGNAL(statusChanged(QnResource::Status, QnResource::Status)), this, SLOT(at_camera_statusChanged()));

    tryInitialize(camera);
}

void QnWorkbenchPtzController::at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera) {
    m_dataByCamera.remove(camera);

    qnRemoveValues(m_cameraByHandle, camera);
    qnRemoveValues(m_cameraByTimerId, camera);

    disconnect(camera.data(), NULL, this, NULL);
}

void QnWorkbenchPtzController::at_camera_statusChanged(const QnVirtualCameraResourcePtr &camera) {
    tryInitialize(camera);
}

void QnWorkbenchPtzController::at_camera_statusChanged() {
    at_camera_statusChanged(toSharedPointer(checked_cast<QnVirtualCameraResource *>(sender())));
}

void QnWorkbenchPtzController::at_ptzGetPosition_replyReceived(int status, qreal xPos, qreal yPox, qreal zoomPos, int handle) {
    QnVirtualCameraResourcePtr camera = m_cameraByHandle.value(handle);
    if(!camera)
        return; /* Already removed from the pool. */

    m_cameraByHandle.remove(handle);
    
    PtzData &data = m_dataByCamera[camera];
    if(status == 0) {
        data.position = QVector3D(xPos, yPox, zoomPos);
        data.attemptCount[GetPositionRequest] = 0;
    } else {
        if(data.attemptCount > maxAttempts) {
            qnWarning("Could not get PTZ position from '%1' after %2 attempts, giving up.", camera->getName(), maxAttempts);
        } else {
            sendGetPosition(camera);
        }
    }
}

void QnWorkbenchPtzController::at_ptzSetPosition_replyReceived(int status, int handle) {
    QnVirtualCameraResourcePtr camera = m_cameraByHandle.value(handle);
    if(!camera)
        return; /* Already removed from the pool. */

    m_cameraByHandle.remove(handle);
    QVector3D position = m_requestByHandle[handle];
    m_requestByHandle.remove(handle);

    PtzData &data = m_dataByCamera[camera];
    if(status == 0) {
        data.position = position;
        data.attemptCount[SetPositionRequest] = 0;
    } else {
        if(data.attemptCount > maxAttempts) {
            qnWarning("Could not set PTZ position for '%1' after %2 attempts, giving up.", camera->getName(), maxAttempts);
        } else {
            sendSetPosition(camera, position);
        }
    }
}

void QnWorkbenchPtzController::at_ptzSetMovement_replyReceived(int status, int handle) {
    QnVirtualCameraResourcePtr camera = m_cameraByHandle.value(handle);
    if(!camera)
        return; /* Already removed from the pool. */

    m_cameraByHandle.remove(handle);
    QVector3D movement = m_requestByHandle[handle];
    m_requestByHandle.remove(handle);

    PtzData &data = m_dataByCamera[camera];
    if(status == 0) {
        data.movement = movement;
        data.attemptCount[SetMovementRequest] = 0;
    } else {
        if(data.attemptCount > maxAttempts) {
            qnWarning("Could not set PTZ movement for '%1' after %2 attempts, giving up.", camera->getName(), maxAttempts);
        } else {
            sendSetMovement(camera, movement);
        }
    }
}

