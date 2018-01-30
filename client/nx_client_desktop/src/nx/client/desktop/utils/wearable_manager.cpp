#include "wearable_manager.h"

#include <core/resource/security_cam_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include "wearable_worker.h"

namespace nx {
namespace client {
namespace desktop {

struct WearableManager::Private
{
    QnUserResourcePtr currentUser;
    QHash<QnUuid, WearableWorker*> workers;
};

WearableManager::WearableManager(QObject* parent):
    QObject(parent),
    QnCommonModuleAware(parent),
    d(new Private)
{
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this, &WearableManager::dropWorker);
}

WearableManager::~WearableManager()
{
    dropAllWorkers();
}

void WearableManager::setCurrentUser(const QnUserResourcePtr& user)
{
    if (d->currentUser == user)
        return;

    dropAllWorkers();
    d->currentUser = user;
}

const QnUserResourcePtr& WearableManager::currentUser() const
{
    return d->currentUser;
}

WearableState WearableManager::state(const QnSecurityCamResourcePtr& camera)
{
    if (!camera)
        return WearableState();

    auto pos = d->workers.find(camera->getId());
    if (pos == d->workers.end())
        return WearableState();

    return (*pos)->state();
}

QList<WearableState> WearableManager::runningUploads()
{
    QList<WearableState> result;
    for (WearableWorker* worker : d->workers)
        if (worker->isWorking())
            result.push_back(worker->state());
    return result;
}

void WearableManager::updateState(const QnSecurityCamResourcePtr& camera)
{
    if(WearableWorker* worker = ensureWorker(camera))
        worker->updateState();
}

bool WearableManager::addUpload(const QnSecurityCamResourcePtr& camera, const QString& path, QString* errorMessage)
{
    if (WearableWorker* worker = ensureWorker(camera))
        return worker->addUpload(path, errorMessage);
    return true; //< Just ignore it silently.
}

void WearableManager::cancelAllUploads()
{
    dropAllWorkers();
}

WearableWorker* WearableManager::ensureWorker(const QnSecurityCamResourcePtr& camera)
{
    NX_ASSERT(d->currentUser);

    // If you're very lucky you may end up with dangling cameras getting passed in here.
    if (!camera->getParentServer())
        return nullptr;

    QnUuid cameraId = camera->getId();

    auto pos = d->workers.find(cameraId);
    if (pos != d->workers.end())
        return *pos;

    WearableWorker* result = new WearableWorker(camera, d->currentUser, this);
    d->workers[cameraId] = result;

    connect(result, &WearableWorker::error, this, &WearableManager::error);
    connect(result, &WearableWorker::stateChanged, this, &WearableManager::stateChanged);

    return result;
}

void WearableManager::dropWorker(const QnResourcePtr& resource)
{
    auto pos = d->workers.find(resource->getId());
    if (pos == d->workers.end())
        return;

    delete *pos;
    d->workers.erase(pos);
}

void WearableManager::dropAllWorkers()
{
    qDeleteAll(d->workers);
    d->workers.clear();
}

} // namespace desktop
} // namespace client
} // namespace nx
