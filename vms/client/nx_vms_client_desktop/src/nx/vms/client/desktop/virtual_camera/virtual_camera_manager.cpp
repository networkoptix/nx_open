// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "virtual_camera_manager.h"

#include <core/resource/security_cam_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/desktop/system_context.h>
#include <recording/time_period_list.h>

#include "virtual_camera_preparer.h"
#include "virtual_camera_worker.h"

namespace nx::vms::client::desktop {

struct VirtualCameraManager::Private
{
    QnUserResourcePtr currentUser;
    QHash<QnUuid, VirtualCameraWorker*> workers;
};

VirtualCameraManager::VirtualCameraManager(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private)
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, &VirtualCameraManager::dropWorker);
}

VirtualCameraManager::~VirtualCameraManager()
{
    dropAllWorkers();
}

void VirtualCameraManager::setCurrentUser(const QnUserResourcePtr& user)
{
    if (d->currentUser == user)
        return;

    dropAllWorkers();
    d->currentUser = user;
}

const QnUserResourcePtr& VirtualCameraManager::currentUser() const
{
    return d->currentUser;
}

VirtualCameraState VirtualCameraManager::state(const QnSecurityCamResourcePtr& camera)
{
    if (!camera)
        return VirtualCameraState();

    auto pos = d->workers.find(camera->getId());
    if (pos == d->workers.end())
        return VirtualCameraState();

    return (*pos)->state();
}

QList<VirtualCameraState> VirtualCameraManager::runningUploads()
{
    QList<VirtualCameraState> result;
    for (VirtualCameraWorker* worker : d->workers)
        if (worker->isWorking())
            result.push_back(worker->state());
    return result;
}

void VirtualCameraManager::prepareUploads(
    const QnSecurityCamResourcePtr& camera,
    const QStringList& filePaths,
    QObject* target,
    std::function<void(const VirtualCameraUpload&)> callback)
{
    if (!camera->getParentServer())
        return;

    VirtualCameraPreparer* checker = new VirtualCameraPreparer(camera, this);

    connect(checker, &VirtualCameraPreparer::finished, target, callback);
    connect(checker, &VirtualCameraPreparer::finished, checker, &QObject::deleteLater);

    checker->prepareUploads(filePaths, state(camera).periods());
}

void VirtualCameraManager::updateState(const QnSecurityCamResourcePtr& camera)
{
    if(VirtualCameraWorker* worker = cameraWorker(camera))
        worker->updateState();
}

bool VirtualCameraManager::addUpload(const QnSecurityCamResourcePtr& camera, const VirtualCameraPayloadList& payloads)
{
    if (VirtualCameraWorker* worker = cameraWorker(camera))
        return worker->addUpload(payloads);
    return true; //< Just ignore it silently.
}

void VirtualCameraManager::cancelUploads(const QnSecurityCamResourcePtr& camera)
{
    QnUuid cameraId = camera->getId();

    if (!d->workers.contains(cameraId))
        return;

    d->workers[cameraId]->cancel();
}

void VirtualCameraManager::cancelAllUploads()
{
    dropAllWorkers();
}

VirtualCameraWorker* VirtualCameraManager::cameraWorker(const QnSecurityCamResourcePtr& camera)
{
    NX_ASSERT(d->currentUser);

    // If you're very lucky you may end up with dangling cameras getting passed in here.
    if (!camera->getParentServer())
        return nullptr;

    QnUuid cameraId = camera->getId();

    auto pos = d->workers.find(cameraId);
    if (pos != d->workers.end())
        return *pos;

    VirtualCameraWorker* result = new VirtualCameraWorker(camera, d->currentUser, this);
    d->workers[cameraId] = result;

    connect(result, &VirtualCameraWorker::error, this, &VirtualCameraManager::error);
    connect(result, &VirtualCameraWorker::stateChanged, this, &VirtualCameraManager::stateChanged);
    connect(result, &VirtualCameraWorker::finished, this,
        [this, result, cameraId]()
        {
            result->deleteLater();
            d->workers.remove(cameraId);
        });

    return result;
}

void VirtualCameraManager::dropWorker(const QnResourcePtr& resource)
{
    auto pos = d->workers.find(resource->getId());
    if (pos == d->workers.end())
        return;
    VirtualCameraWorker* worker = *pos;

    disconnect(worker, nullptr, this, nullptr);
    delete worker;

    d->workers.erase(pos);
}

void VirtualCameraManager::dropAllWorkers()
{
    qDeleteAll(d->workers);
    d->workers.clear();
}

} // namespace nx::vms::client::desktop
