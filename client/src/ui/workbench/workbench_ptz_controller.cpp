#include "workbench_ptz_controller.h"

#include <utils/common/math.h>
#include <utils/common/checked_cast.h>
#include <utils/common/space_mapper.h>

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


// -------------------------------------------------------------------------- //
// QnWorkbenchPtzController
// -------------------------------------------------------------------------- //
QnWorkbenchPtzController::PtzData::PtzData(): 
    initialized(false),
    position(qQNaN<QVector3D>()),
    physicalPosition(qQNaN<QVector3D>()),
    movement(QVector3D())
{
    memset(attemptCount, 0, sizeof(attemptCount));
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


    /* 
     Get width from here:
     http://en.wikipedia.org/wiki/Image_sensor_format
     Note: 1/2.8" is 5.2mm x 3.9mm, D=6.5mm.

     Calculate width-based crop factor C = 36/W.

     Multiply focal lengths by width-based crop factor to get width-based equivalent focal length.
    */

    // TODO: make externally configurable.
    QVector<QPair<qreal, qreal> > toCameraY;
    toCameraY.push_back(qMakePair(-0.5, -90.0));
    toCameraY.push_back(qMakePair(0.0, 0.0));
    toCameraY.push_back(qMakePair(1.0, 20.0));

    const qreal cropFactor = 7.92;//6.92;
    m_mapperByModel[QLatin1String("Q6035-E")] = new QnPtzSpaceMapper(
        QnVectorSpaceMapper(
            QnScalarSpaceMapper(-1, 1, 0, 360, Qn::PeriodicExtrapolation),
            QnScalarSpaceMapper(0.111, -0.5, 20, -90, Qn::ConstantExtrapolation),
            QnScalarSpaceMapper(0, 1, 4.7 * cropFactor, 94 * cropFactor, Qn::ConstantExtrapolation)
        ),
        QnVectorSpaceMapper(
            QnScalarSpaceMapper(-1, 1, 0, 360, Qn::PeriodicExtrapolation),
            QnScalarSpaceMapper(toCameraY, Qn::ConstantExtrapolation),
            QnScalarSpaceMapper(0, 1, 4.7 * cropFactor, 94 * cropFactor, Qn::ConstantExtrapolation)
        )
    );
}

QnWorkbenchPtzController::~QnWorkbenchPtzController() {
    qDeleteAll(m_mapperByModel);
    m_mapperByModel.clear();

    QnWorkbenchPtzCamerasWatcher *watcher = context()->instance<QnWorkbenchPtzCamerasWatcher>();
    foreach(const QnVirtualCameraResourcePtr &camera, watcher->ptzCameras())
        at_ptzCameraWatcher_ptzCameraRemoved(camera);
    disconnect(watcher, NULL, this, NULL);
}

QVector3D QnWorkbenchPtzController::position(const QnVirtualCameraResourcePtr &camera) const {
    if(!camera) {
        qnNullWarning(camera);
        return qQNaN<QVector3D>();
    }

    QHash<QnVirtualCameraResourcePtr, PtzData>::const_iterator pos = m_dataByCamera.find(camera);
    if(pos == m_dataByCamera.end())
        return qQNaN<QVector3D>();

    return pos->position;
}

QVector3D QnWorkbenchPtzController::physicalPosition(const QnVirtualCameraResourcePtr &camera) const {
    if(!camera) {
        qnNullWarning(camera);
        return qQNaN<QVector3D>();
    }

    QHash<QnVirtualCameraResourcePtr, PtzData>::const_iterator pos = m_dataByCamera.find(camera);
    if(pos == m_dataByCamera.end())
        return qQNaN<QVector3D>();

    return pos->physicalPosition;
}

void QnWorkbenchPtzController::setPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &position) {
    if(!camera) {
        qnNullWarning(camera);
        return;
    }

    sendSetPosition(camera, position);
}

void QnWorkbenchPtzController::setPhysicalPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &physicalPosition) {
    if(!camera) {
        qnNullWarning(camera);
        return;
    }

    const QnPtzSpaceMapper *mapper = this->mapper(camera);
    if(!mapper) {
        qnWarning("Physical position is not supported for camera '%1'.", camera->getName());
        return;
    }

    setPosition(camera, mapper->toCamera.physicalToLogical(physicalPosition));
}

QVector3D QnWorkbenchPtzController::movement(const QnVirtualCameraResourcePtr &camera) const {
    if(!camera) {
        qnNullWarning(camera);
        return QVector3D();
    }

    QHash<QnVirtualCameraResourcePtr, PtzData>::const_iterator pos = m_dataByCamera.find(camera);
    if(pos == m_dataByCamera.end())
        return QVector3D();

    return pos->movement;
}

void QnWorkbenchPtzController::setMovement(const QnVirtualCameraResourcePtr &camera, const QVector3D &motion) {
    if(!camera) {
        qnNullWarning(camera);
        return;
    }

    sendSetMovement(camera, motion);
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

    int handle = server->apiConnection()->asyncPtzMoveTo(camera, position.x(), position.y(), position.z(), this, SLOT(at_ptzSetPosition_replyReceived(int, int)));
    m_cameraByHandle[handle] = camera;
    m_requestByHandle[handle] = position;

    PtzData &data = m_dataByCamera[camera];
    data.attemptCount[SetPositionRequest]++;
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

const QnPtzSpaceMapper *QnWorkbenchPtzController::mapper(const QnVirtualCameraResourcePtr &camera) const {
    return m_mapperByModel.value(camera->getModel());
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
        if(qFuzzyIsNull(data.movement)) {
            data.position = QVector3D(xPos, yPox, zoomPos);
            if(const QnPtzSpaceMapper *mapper = this->mapper(camera))
                data.physicalPosition = mapper->fromCamera.logicalToPhysical(data.position);
        }
        data.attemptCount[GetPositionRequest] = 0;
    } else {
        if(data.attemptCount[GetPositionRequest] > maxAttempts) {
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
        if(const QnPtzSpaceMapper *mapper = this->mapper(camera))
            data.physicalPosition = mapper->toCamera.logicalToPhysical(data.position);
        data.movement = QVector3D();
        data.attemptCount[SetPositionRequest] = 0;
    } else {
        if(data.attemptCount[SetPositionRequest] > maxAttempts) {
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
        if(!qFuzzyIsNull(movement)) {
            data.position = qQNaN<QVector3D>();
            data.physicalPosition = qQNaN<QVector3D>();
        }
        data.movement = movement;
        data.attemptCount[SetMovementRequest] = 0;
    } else {
        if(data.attemptCount[SetMovementRequest] > maxAttempts) {
            qnWarning("Could not set PTZ movement for '%1' after %2 attempts, giving up.", camera->getName(), maxAttempts);
        } else {
            sendSetMovement(camera, movement);
        }
    }
}

