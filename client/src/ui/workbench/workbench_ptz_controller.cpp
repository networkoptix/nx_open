#include "workbench_ptz_controller.h"

#include <utils/math/math.h>
#include <utils/common/checked_cast.h>
#include <utils/math/space_mapper.h>

#include <api/media_server_connection.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/workbench/watchers/workbench_ptz_mapper_watcher.h>
#include <ui/workbench/watchers/workbench_ptz_camera_watcher.h>
#include <ui/workbench/workbench_context.h>

//#define QN_WORKBENCH_PTZ_CONTROLLER_DEBUG
#ifdef QN_WORKBENCH_PTZ_CONTROLLER_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__;
#else
#   define TRACE(...)
#endif


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
    QnWorkbenchContextAware(parent),
    m_mapperWatcher(context()->instance<QnWorkbenchPtzMapperWatcher>())
{
    QnWorkbenchPtzCameraWatcher *watcher = context()->instance<QnWorkbenchPtzCameraWatcher>();
    connect(watcher, SIGNAL(ptzCameraAdded(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &)));
    connect(watcher, SIGNAL(ptzCameraRemoved(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &)));
    foreach(const QnVirtualCameraResourcePtr &camera, watcher->ptzCameras())
        at_ptzCameraWatcher_ptzCameraAdded(camera);
}

QnWorkbenchPtzController::~QnWorkbenchPtzController() {
    QnWorkbenchPtzCameraWatcher *watcher = context()->instance<QnWorkbenchPtzCameraWatcher>();
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

    const QnPtzSpaceMapper *mapper = m_mapperWatcher->mapper(camera);
    if(!mapper) {
        qnWarning("Physical position is not supported for camera '%1'.", camera->getName());
        return;
    }

    setPosition(camera, mapper->toCamera().physicalToLogical(physicalPosition));
}

void QnWorkbenchPtzController::updatePosition(const QnVirtualCameraResourcePtr &camera) {
    sendGetPosition(camera);
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
    if(!server)
        return; // TODO. This really does happen

    PtzData &data = m_dataByCamera[camera];
    data.attemptCount[GetPositionRequest]++;

    int handle = server->apiConnection()->ptzGetPosAsync(camera, this, SLOT(at_ptzGetPosition_replyReceived(int, const QVector3D &, int)));
    m_cameraByHandle[handle] = camera;
}

void QnWorkbenchPtzController::sendSetPosition(const QnVirtualCameraResourcePtr &camera, const QVector3D &position) {
    QnMediaServerResourcePtr server = resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();
    if(!server)
        return; // TODO. This really does happen

    TRACE("SENT POSITION" << position);

    PtzData &data = m_dataByCamera[camera];
    data.sequenceNumber++;
    data.attemptCount[SetPositionRequest]++;

    int handle = server->apiConnection()->ptzMoveToAsync(camera, position, data.sequenceId, data.sequenceNumber, this, SLOT(at_ptzSetPosition_replyReceived(int, int)));
    m_cameraByHandle[handle] = camera;
    m_requestByHandle[handle] = position;
}

void QnWorkbenchPtzController::sendSetMovement(const QnVirtualCameraResourcePtr &camera, const QVector3D &movement) {
    QnMediaServerResourcePtr server = resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();
    if(!server)
        return; // TODO. This really does happen

    PtzData &data = m_dataByCamera[camera];
    data.sequenceNumber++;
    data.attemptCount[SetMovementRequest]++;

    int handle;
    if(qFuzzyIsNull(movement)) {
        handle = server->apiConnection()->ptzStopAsync(camera, data.sequenceId, data.sequenceNumber, this, SLOT(at_ptzSetMovement_replyReceived(int, int)));
    } else {
        handle = server->apiConnection()->ptzMoveAsync(camera, movement, data.sequenceId, data.sequenceNumber, this, SLOT(at_ptzSetMovement_replyReceived(int, int)));
    }
    m_cameraByHandle[handle] = camera;
    m_requestByHandle[handle] = movement;
}

void QnWorkbenchPtzController::tryInitialize(const QnVirtualCameraResourcePtr &camera) {
    PtzData &data = m_dataByCamera[camera];
    if(data.initialized)
        return;

    QnResource::Status status = camera->getStatus();
    if(status == QnResource::Offline || status == QnResource::Unauthorized)
        return;

    data.initialized = true;
    data.sequenceId = QUuid::createUuid();
    data.sequenceNumber = 0;
    sendGetPosition(camera);
    sendSetMovement(camera, QVector3D());
}

void QnWorkbenchPtzController::emitChanged(const QnVirtualCameraResourcePtr &camera, bool position, bool movement) {
    if(position)
        emit positionChanged(camera);
    if(movement)
        emit movementChanged(camera);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchPtzController::at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera) {
    connect(camera.data(), SIGNAL(statusChanged(const QnResourcePtr &)), this, SLOT(at_resource_statusChanged(const QnResourcePtr &)));

    tryInitialize(camera);
}

void QnWorkbenchPtzController::at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera) {
    m_dataByCamera.remove(camera);

    qnRemoveValues(m_cameraByHandle, camera);
    qnRemoveValues(m_cameraByTimerId, camera);

    disconnect(camera.data(), NULL, this, NULL);
}

void QnWorkbenchPtzController::at_resource_statusChanged(const QnResourcePtr &resource) {
    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
        tryInitialize(camera);
}

void QnWorkbenchPtzController::at_ptzGetPosition_replyReceived(int status, const QVector3D &position, int handle) {
    QnVirtualCameraResourcePtr camera = m_cameraByHandle.value(handle);
    if(!camera)
        return; /* Already removed from the pool. */

    TRACE("GOT POSITION" << position);

    m_cameraByHandle.remove(handle);
    
    PtzData &data = m_dataByCamera[camera];
    if(status == 0) {
        bool positionChanged = false;
        if(qFuzzyIsNull(data.movement)) {
            positionChanged = !qFuzzyCompare(position, data.position);
            data.position = position;
            if(const QnPtzSpaceMapper *mapper = m_mapperWatcher->mapper(camera))
                data.physicalPosition = mapper->fromCamera().logicalToPhysical(data.position);
        }
        data.attemptCount[GetPositionRequest] = 0;

        emitChanged(camera, positionChanged, false);
    } else {
        if(data.attemptCount[GetPositionRequest] > maxAttempts) {
//            qnWarning("Could not get PTZ position from '%1' after %2 attempts, giving up.", camera->getName(), maxAttempts);
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
        bool positionChanged = !qFuzzyCompare(data.position, position);
        data.position = position;
        if(const QnPtzSpaceMapper *mapper = m_mapperWatcher->mapper(camera))
            data.physicalPosition = mapper->toCamera().logicalToPhysical(data.position);

        bool movementChanged = qFuzzyCompare(data.movement, QVector3D());
        data.movement = QVector3D();
        data.attemptCount[SetPositionRequest] = 0;

        emitChanged(camera, positionChanged, movementChanged);
    } else {
        if(data.attemptCount[SetPositionRequest] > maxAttempts) {
//            qnWarning("Could not set PTZ position for '%1' after %2 attempts, giving up.", camera->getName(), maxAttempts);
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
        bool positionChanged = false;
        if(!qFuzzyIsNull(movement)) {
            positionChanged = !qIsNaN(data.position);
            data.position = qQNaN<QVector3D>();
            data.physicalPosition = qQNaN<QVector3D>();
        } else if(qIsNaN(data.position)) {
            sendGetPosition(camera);
        }
        
        bool movementChanged = !qFuzzyCompare(data.movement, movement);
        data.movement = movement;
        data.attemptCount[SetMovementRequest] = 0;

        emitChanged(camera, positionChanged, movementChanged);
    } else {
        if(data.attemptCount[SetMovementRequest] > maxAttempts) {
//            qnWarning("Could not set PTZ movement for '%1' after %2 attempts, giving up.", camera->getName(), maxAttempts);
        } else {
            sendSetMovement(camera, movement);
        }
    }
}

