#include "ptz_controller_pool.h"

#include <QtCore/QMutex>
#include <QtCore/QThreadPool>
#include <QtCore/QAtomicInt>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include "abstract_ptz_controller.h"
#include "proxy_ptz_controller.h"

// -------------------------------------------------------------------------- //
// QnPtzControllerPoolPrivate
// -------------------------------------------------------------------------- //
class QnPtzControllerPoolPrivate {
public:
    QnPtzControllerPoolPrivate(): mode(QnPtzControllerPool::NormalControllerConstruction), executorThread(NULL), commandThreadPool(NULL) {}

    void updateController(const QnResourcePtr &resource);

    mutable QMutex mutex;
    QAtomicInt mode;
    QHash<QnResourcePtr, QnPtzControllerPtr> controllerByResource;
    QThread *executorThread;
    QThreadPool *commandThreadPool;
    QnResourcePool *resourcePool;
    QnPtzControllerPool *q;
};


// -------------------------------------------------------------------------- //
// QnPtzControllerPoolPrivate
// -------------------------------------------------------------------------- //
class QnPtzControllerCreationCommand: public QRunnable {
public:
    QnPtzControllerCreationCommand(const QnResourcePtr &resource, QnPtzControllerPool *pool):
        m_resource(resource),
        m_pool(pool)
    {}

    virtual void run() override {
        emit m_pool->d->updateController(m_resource);
    }

private:
    QnResourcePtr m_resource;
    QnPtzControllerPool *m_pool;
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

    d->commandThreadPool = new QThreadPool(this);
    d->commandThreadPool->setMaxThreadCount(128); /* This should be enough. */

    connect(d->resourcePool,    &QnResourcePool::resourceAdded,             this,   &QnPtzControllerPool::registerResource);
    connect(d->resourcePool,    &QnResourcePool::resourceRemoved,           this,   &QnPtzControllerPool::unregisterResource);
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

QThreadPool *QnPtzControllerPool::commandThreadPool() const {
    return d->commandThreadPool;
}

QnPtzControllerPool::ControllerConstructionMode QnPtzControllerPool::constructionMode() const {
    return static_cast<ControllerConstructionMode>(d->mode.loadAcquire());
}

void QnPtzControllerPool::setConstructionMode(ControllerConstructionMode mode) {
    d->mode.storeRelease(mode);
}

QnPtzControllerPtr QnPtzControllerPool::controller(const QnResourcePtr &resource) const {
    QMutexLocker locker(&d->mutex);
    return d->controllerByResource.value(resource);
}

QnPtzControllerPtr QnPtzControllerPool::createController(const QnResourcePtr &) const {
    return QnPtzControllerPtr();
}

void QnPtzControllerPool::registerResource(const QnResourcePtr &resource) {
    updateController(resource);
}

void QnPtzControllerPool::unregisterResource(const QnResourcePtr &resource) {
    bool changed = false;
    {
        QMutexLocker locker(&d->mutex);

        changed = d->controllerByResource.remove(resource) > 0;
    }

    if(changed)
        emit controllerChanged(resource);
}

void QnPtzControllerPool::updateController(const QnResourcePtr &resource) {
    if(d->mode.loadAcquire() == NormalControllerConstruction) {
        d->updateController(resource);
    } else {
        QRunnable *command = new QnPtzControllerCreationCommand(resource, this);
        command->setAutoDelete(true);
        d->commandThreadPool->start(command);
    }
}

void QnPtzControllerPoolPrivate::updateController(const QnResourcePtr &resource) {
    QnPtzControllerPtr controller = q->createController(resource);
    {
        QMutexLocker locker(&mutex);
        if(controllerByResource.value(resource) == controller)
            return;

        controllerByResource.insert(resource, controller);
    }

    /* Some controller require an event loop to function, so we move them
     * to executor thread. Note that controllers don't run synchronous requests 
     * in their associated thread, so this won't present any problems for
     * other users of the executor thread. */
    while(controller) {
        controller->moveToThread(executorThread);
        if(QnProxyPtzControllerPtr proxyController = controller.dynamicCast<QnProxyPtzController>()) {
            controller = proxyController->baseController();
        } else {
            controller.clear();
        }
    }

    emit q->controllerChanged(resource);
}

