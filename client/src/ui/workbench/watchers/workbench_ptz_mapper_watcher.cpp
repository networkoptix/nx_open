#include "workbench_ptz_mapper_watcher.h"

#include <cassert>

#include <utils/common/space_mapper.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_ptz_camera_watcher.h>

QnWorkbenchPtzMapperWatcher::QnWorkbenchPtzMapperWatcher(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    QnWorkbenchPtzCameraWatcher *ptzCamerasWatcher = context()->instance<QnWorkbenchPtzCameraWatcher>();
    
    connect(ptzCamerasWatcher, SIGNAL(ptzCameraAdded(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &)));
    connect(ptzCamerasWatcher, SIGNAL(ptzCameraRemoved(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &)));

    foreach(const QnVirtualCameraResourcePtr &resource, ptzCamerasWatcher->ptzCameras())
        at_ptzWatcher_ptzCameraAdded(resource);
}

QnWorkbenchPtzMapperWatcher::~QnWorkbenchPtzMapperWatcher() {
    QnWorkbenchPtzCameraWatcher *ptzCamerasWatcher = context()->instance<QnWorkbenchPtzCameraWatcher>();

    foreach(const QnVirtualCameraResourcePtr &resource, ptzCamerasWatcher->ptzCameras())
        at_ptzWatcher_ptzCameraRemoved(resource);
    assert(m_mapperByResource.isEmpty());

    disconnect(ptzCamerasWatcher, NULL, this, NULL);
}

const QnPtzSpaceMapper *QnWorkbenchPtzMapperWatcher::mapper(const QnVirtualCameraResourcePtr &resource) const {
    return m_mapperByResource.value(resource);
}

void QnWorkbenchPtzMapperWatcher::addMapper(const QnVirtualCameraResourcePtr &resource, const QnPtzSpaceMapper *mapper) {
    delete m_mapperByResource.value(resource, NULL);

    m_mapperByResource.insert(resource, mapper);
}

void QnWorkbenchPtzMapperWatcher::removeMapper(const QnVirtualCameraResourcePtr &resource) {
    delete m_mapperByResource.value(resource, NULL);

    m_mapperByResource.remove(resource);
}

void QnWorkbenchPtzMapperWatcher::sendRequest(const QnMediaServerResourcePtr &server, const QnVirtualCameraResourcePtr &camera) {
    int handle = server->apiConnection()->asyncPtzGetSpaceMapper(camera, this, SLOT(at_replyReceived(int, const QnPtzSpaceMapper &, int)));
    m_requests.insert(camera);
    m_resourceByHandle.insert(handle, camera);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchPtzMapperWatcher::at_ptzWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera) {
    addMapper(camera, NULL);

    connect(camera, SIGNAL(statusChanged(const QnResourcePtr &)), this, SLOT(at_resource_statusChanged(const QnResourcePtr &)));
    at_resource_statusChanged(camera);
}

void QnWorkbenchPtzMapperWatcher::at_ptzWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera) {
    disconnect(camera, NULL, this, NULL);

    removeMapper(camera);
}

void QnWorkbenchPtzMapperWatcher::at_resource_statusChanged(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return;

    QnResource::Status status = resource->getStatus();
    if(status == QnResource::Online && status == QnResource::Recording)
        return;

    if(m_requests.contains(camera))
        return; /* No duplicate requests. */

    QnMediaServerResourcePtr server = resource->getParentResource().dynamicCast<QnMediaServerResource>();
    if(!server)
        return;

    QnResource::Status serverStatus = server->getStatus();
    if(serverStatus != QnResource::Online)
        return;

    sendRequest(server, camera);
}

void QnWorkbenchPtzMapperWatcher::at_replyReceived(int status, const QnPtzSpaceMapper &mapper, int handle) {
    Q_UNUSED(status);

    QnVirtualCameraResourcePtr camera = m_resourceByHandle.value(handle);
    m_resourceByHandle.remove(handle);
    m_requests.remove(camera);

    if(!m_mapperByResource.contains(camera))
        return; /* We're not watching this camera anymore. */

    addMapper(camera, mapper.isNull() ? NULL : new QnPtzSpaceMapper(mapper));

    disconnect(camera, NULL, this, NULL); /* Don't request for mapper again. */
}



