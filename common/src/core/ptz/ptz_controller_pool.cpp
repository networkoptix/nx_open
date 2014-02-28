#include "ptz_controller_pool.h"

#include <QtCore/QMutex>

#include <core/resource_management/resource_pool.h>

// -------------------------------------------------------------------------- //
// QnPtzControllerPoolPrivate
// -------------------------------------------------------------------------- //
class QnPtzControllerPoolPrivate {
public:
    QnPtzControllerPoolPrivate(): executorThread(NULL) {}

    bool registerResource(const QnResourcePtr &resource, QnPtzControllerPtr *controller) {
        {
            QMutexLocker locker(&mutex);

            if(controllerByResource.contains(resource)) {
                *controller = controllerByResource.value(resource);
                return false; /* Already registered. */
            }
        }

        *controller = q->createController(resource);

        {
            QMutexLocker locker(&mutex);

            if(controllerByResource.contains(resource)) {
                *controller = controllerByResource.value(resource);
                return false; /* Already registered. */
            }

            controllerByResource.insert(resource, *controller);
        }

        return true;
    }

    bool unregisterResource(const QnResourcePtr &resource) {
        bool changed = false;
        {
            QMutexLocker locker(&mutex);
            changed = !controllerByResource.value(resource).isNull();
            controllerByResource.remove(resource);
        }

        return changed;
    }

    bool updateResource(const QnResourcePtr &resource) {
        bool changed = false;
        QnPtzControllerPtr controller = q->createController(resource);

        {
            QMutexLocker locker(&mutex);

            changed = controllerByResource.value(resource) != controller;
            controllerByResource.insert(resource, controller);
        }

        return changed;
    }

    bool getController(const QnResourcePtr &resource, QnPtzControllerPtr *controller) {
        QMutexLocker locker(&mutex);

        *controller = controllerByResource.value(resource);
        return controllerByResource.contains(resource);
    }

public:
    mutable QMutex mutex;
    QHash<QnResourcePtr, QnPtzControllerPtr> controllerByResource;
    QThread *executorThread;
    QnResourcePool *resourcePool;
    QnPtzControllerPool *q;
};


// -------------------------------------------------------------------------- //
// QnPtzControllerPool
// -------------------------------------------------------------------------- //
QnPtzControllerPool::QnPtzControllerPool(QObject *parent):
    base_type(parent),
    d(new QnPtzControllerPoolPrivate())
{
    d->q = this;
    d->resourcePool = qnResPool;

    d->executorThread = new QThread(this);
    d->executorThread->start();

    /* QueuedConnection is important here because we want to get into 
     * registration from the event loop as some controllers require an 
     * event loop to function properly. */
    connect(d->resourcePool,    &QnResourcePool::resourceAdded,   this,   &QnPtzControllerPool::registerResource,   Qt::QueuedConnection);
    connect(d->resourcePool,    &QnResourcePool::resourceRemoved, this,   &QnPtzControllerPool::unregisterResource, Qt::QueuedConnection);
    foreach(const QnResourcePtr &resource, d->resourcePool->getResources())
        registerResource(resource);
}

QnPtzControllerPool::~QnPtzControllerPool() {
    while(!d->controllerByResource.isEmpty())
        unregisterResource(d->controllerByResource.begin().key());

    d->executorThread->exit();
    d->executorThread->wait();
}

QThread *QnPtzControllerPool::executorThread() const {
    return d->executorThread;
}

QnPtzControllerPtr QnPtzControllerPool::controller(const QnResourcePtr &resource) const {
    QnPtzControllerPtr result;
    if(d->getController(resource, &result))
        return result; /* Note that this one may be NULL. */

    if(!d->resourcePool->getResources().contains(resource))
        return QnPtzControllerPtr();

    // qDebug() << ">>>>>>>> getController before registerResource for" << resource->getName();

    /* Controller is not there because we didn't get the signal yet. */
    if(d->registerResource(resource, &result))
        emit const_cast<QnPtzControllerPool *>(this)->controllerChanged(resource);

    return result;
}

void QnPtzControllerPool::registerResource(const QnResourcePtr &resource) {
    // qDebug() << ">>>>>>>> registerResource for" << resource->getName();

    QnPtzControllerPtr controller;
    if(d->registerResource(resource, &controller))
        emit controllerChanged(resource);
}

void QnPtzControllerPool::unregisterResource(const QnResourcePtr &resource) {
    // qDebug() << ">>>>>>>> unregisterResource for" << resource->getName();

    if(d->unregisterResource(resource))
        emit controllerChanged(resource);
}

QnPtzControllerPtr QnPtzControllerPool::createController(const QnResourcePtr &) const {
    return QnPtzControllerPtr();
}

void QnPtzControllerPool::updateController(const QnResourcePtr &resource) {
    qDebug() << ">>>>>>>> updateController for" << resource->getName();

    if(d->updateResource(resource))
        emit controllerChanged(resource);
}
