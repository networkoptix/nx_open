#include "wearable_manager.h"

#include <core/resource/security_cam_resource.h>
#include <core/resource/user_resource.h>

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
    d(new Private)
{

}

WearableManager::~WearableManager()
{
    clearAllWorkers();
}

void WearableManager::setCurrentUser(const QnUserResourcePtr& user)
{
    if (d->currentUser == user)
        return;

    clearAllWorkers();
    d->currentUser = user;
}

const QnUserResourcePtr& WearableManager::currentUser() const
{
    return d->currentUser;
}

WearableState WearableManager::state(const QnSecurityCamResourcePtr& camera)
{
    auto pos = d->workers.find(camera->getId());
    if (pos == d->workers.end())
        return WearableState();

    return (*pos)->state();
}

void WearableManager::updateState(const QnSecurityCamResourcePtr& camera)
{
    ensureWorker(camera)->updateState();
}

bool WearableManager::addUpload(const QnSecurityCamResourcePtr& camera, const QString& path, QString* errorMessage)
{
    return ensureWorker(camera)->addUpload(path, errorMessage);
}

WearableWorker* WearableManager::ensureWorker(const QnSecurityCamResourcePtr& camera)
{
    NX_ASSERT(d->currentUser);

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

void WearableManager::clearAllWorkers()
{
    qDeleteAll(d->workers);
    d->workers.clear();
}

} // namespace desktop
} // namespace client
} // namespace nx
