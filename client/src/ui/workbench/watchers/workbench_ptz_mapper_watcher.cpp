#include "workbench_ptz_mapper_watcher.h"

#include <cassert>

#include <utils/common/space_mapper.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_ptz_camera_watcher.h>

QnWorkbenchPtzMapperWatcher::QnWorkbenchPtzMapperWatcher(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_ptzCamerasWatcher(context()->instance<QnWorkbenchPtzCameraWatcher>())
{
    connect(m_ptzCamerasWatcher, SIGNAL(ptzCameraAdded(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &)));
    connect(m_ptzCamerasWatcher, SIGNAL(ptzCameraRemoved(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &)));

    foreach(const QnVirtualCameraResourcePtr &resource, m_ptzCamerasWatcher->ptzCameras())
        at_ptzWatcher_ptzCameraAdded(resource);
}

QnWorkbenchPtzMapperWatcher::~QnWorkbenchPtzMapperWatcher() {
    foreach(const QnVirtualCameraResourcePtr &resource, m_ptzCamerasWatcher->ptzCameras())
        at_ptzWatcher_ptzCameraRemoved(resource);
    assert(m_mapperByResource.isEmpty());

    disconnect(m_ptzCamerasWatcher, NULL, this, NULL);
}

void QnWorkbenchPtzMapperWatcher::setMapper(const QnVirtualCameraResourcePtr &resource, const QnPtzSpaceMapper *mapper) {
    const QnPtzSpaceMapper *oldMapper = m_mapperByResource.value(resource, NULL);
    if(oldMapper == mapper)
        return;

    delete oldMapper;
    if(mapper) {
        m_mapperByResource.insert(resource, mapper);
    } else {
        m_mapperByResource.remove(resource);
    }

    emit mapperChanged(resource);
}


const QnPtzSpaceMapper *QnWorkbenchPtzMapperWatcher::mapper(const QnVirtualCameraResourcePtr &resource) const {
    return m_mapperByResource.value(resource);
}

void QnWorkbenchPtzMapperWatcher::sendRequest(const QnMediaServerResourcePtr &server, const QnVirtualCameraResourcePtr &camera) {
    if(camera->getStatus() != QnResource::Online && camera->getStatus() != QnResource::Recording)
        return;

    if(server->getStatus() != QnResource::Online || server->getPrimaryIF().isNull())
        return;

    if(m_requests.contains(camera))
        return; /* No duplicate requests. */

    int handle = server->apiConnection()->asyncPtzGetSpaceMapper(camera, this, SLOT(at_replyReceived(int, const QnPtzSpaceMapper &, int)));
    m_requests.insert(camera);
    m_resourceByHandle.insert(handle, camera);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchPtzMapperWatcher::at_ptzWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera) {
    setMapper(camera, NULL);

    connect(camera, SIGNAL(statusChanged(const QnResourcePtr &)), this, SLOT(at_resource_statusChanged(const QnResourcePtr &)));
    if(QnMediaServerResourcePtr server = camera->getParentResource().dynamicCast<QnMediaServerResource>()) {
        connect(server, SIGNAL(serverIfFound(const QnMediaServerResourcePtr &, const QString &)), this, SLOT(at_server_serverIfFound(const QnMediaServerResourcePtr &)));
        connect(server, SIGNAL(statusChanged(const QnResourcePtr &)), this, SLOT(at_server_statusChanged(const QnResourcePtr &)));
    } else {
        qnWarning("Total fuck up in resource tree: no server for camera '%1'.", camera->getName()); // TODO: #Elric remove once the bug is tracked down.
    }

    at_resource_statusChanged(camera);
}

void QnWorkbenchPtzMapperWatcher::at_ptzWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera) {
    disconnect(camera, NULL, this, NULL);

    setMapper(camera, NULL);
}

void QnWorkbenchPtzMapperWatcher::at_server_serverIfFound(const QnMediaServerResourcePtr &server) {
    foreach(const QnVirtualCameraResourcePtr &camera, m_ptzCamerasWatcher->ptzCameras())
        if(camera->getParentId() == server->getId() && !mapper(camera))
            sendRequest(server, camera);
}

void QnWorkbenchPtzMapperWatcher::at_server_statusChanged(const QnResourcePtr &resource) {
    if(QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>())
        at_server_serverIfFound(server);
}

void QnWorkbenchPtzMapperWatcher::at_resource_statusChanged(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    QnMediaServerResourcePtr server = resource->getParentResource().dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    sendRequest(server, camera);
}

void QnWorkbenchPtzMapperWatcher::at_replyReceived(int status, const QnPtzSpaceMapper &mapper, int handle) {
    Q_UNUSED(status);

    QnVirtualCameraResourcePtr camera = m_resourceByHandle.value(handle);
    m_resourceByHandle.remove(handle);
    m_requests.remove(camera);

    if(!m_ptzCamerasWatcher->ptzCameras().contains(camera))
        return; /* We're not watching this camera anymore. */

    setMapper(camera, mapper.isNull() ? NULL : new QnPtzSpaceMapper(mapper));

    disconnect(camera, NULL, this, NULL); /* Don't request for mapper again. */
}



