#include "ptz_controller_pool.h"

#include <nx/utils/thread/mutex.h>
#include <QtCore/QThreadPool>
#include <QtCore/QAtomicInt>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/log/log.h>
#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>

#include "abstract_ptz_controller.h"
#include "proxy_ptz_controller.h"
#include <common/common_module.h>

#include <utils/common/delete_later.h>

namespace {

void moveControllerToThread(QnPtzControllerPtr controller, QThread* thread)
{
    QnPtzControllerPtr controllerIt = controller;
    while (controllerIt)
    {
        controllerIt->moveToThread(thread);
        if (QnProxyPtzControllerPtr proxyController = controllerIt.dynamicCast<QnProxyPtzController>())
            controllerIt = proxyController->baseController();
        else
            controllerIt.clear();
    }
}

} // namespace

// -------------------------------------------------------------------------- //
// QnPtzControllerPoolPrivate
// -------------------------------------------------------------------------- //
class QnPtzControllerPoolPrivate {
public:
    QnPtzControllerPoolPrivate():
        mode(QnPtzControllerPool::NormalControllerConstruction),
        executorThread(NULL),
        commandThreadPool(NULL),
        deinitialized(false){}

    void updateController(const QnResourcePtr &resource);

    mutable QnMutex mutex;
    QAtomicInt mode;
    QHash<QnResourcePtr, QnPtzControllerPtr> controllerByResource;
    QThread *executorThread;
    QThreadPool *commandThreadPool;
    QnPtzControllerPool *q;
    std::atomic<bool> deinitialized;
};


// -------------------------------------------------------------------------- //
// QnPtzControllerPoolPrivate
// -------------------------------------------------------------------------- //
class QnPtzControllerCreationCommand: public QRunnable
{
public:
    QnPtzControllerCreationCommand(const QnResourcePtr &resource, QnPtzControllerPool *pool):
        m_resource(resource),
        m_pool(pool)
    {}

    virtual void run() override {
        m_pool->d->updateController(m_resource);
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
    QnCommonModuleAware(parent),
    d(new QnPtzControllerPoolPrivate())
{
    d->q = this;

    d->executorThread = new QThread(this);
    d->executorThread->setObjectName( lit("PTZExecutorThread") );
    d->executorThread->start();

    d->commandThreadPool = new QThreadPool(this);
#ifdef __arm__
    const int maxThreads = 8;
#else
    const int maxThreads = 32;
#endif
    d->commandThreadPool->setMaxThreadCount(maxThreads);

    connect(commonModule()->resourcePool(),    &QnResourcePool::resourceAdded,             this,   &QnPtzControllerPool::registerResource);
    connect(commonModule()->resourcePool(),    &QnResourcePool::resourceRemoved,           this,   &QnPtzControllerPool::unregisterResource);
    for(const QnResourcePtr &resource: commonModule()->resourcePool()->getResources())
        registerResource(resource);
}

QnPtzControllerPool::~QnPtzControllerPool()
{
    deinitialize();
}

void QnPtzControllerPool::deinitialize()
{
    if (!d->deinitialized)
    {
        while (!d->controllerByResource.isEmpty())
        {
            auto resourcePtr = d->controllerByResource.begin().key();
            unregisterResource(resourcePtr);
        }

        //have to wait until all posted events have been processed, deleteLater can be called
        //within event slot, that's why we specify second parameter
        WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed(d->executorThread, 3);
        waitingForObjectsToBeFreed.join();

        d->executorThread->exit();
        d->executorThread->wait();

        d->commandThreadPool->clear();
        d->commandThreadPool->waitForDone();

        d->deinitialized = true;
    }
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
    QnMutexLocker locker( &d->mutex );
    return d->controllerByResource.value(resource);
}

QnPtzControllerPtr QnPtzControllerPool::createController(const QnResourcePtr &) const {
    return QnPtzControllerPtr();
}

void QnPtzControllerPool::registerResource(const QnResourcePtr &resource) {
    updateController(resource);
}

void QnPtzControllerPool::unregisterResource(const QnResourcePtr &resource) {
    QnPtzControllerPtr oldController;

    {
        QnMutexLocker locker( &d->mutex );
        oldController = d->controllerByResource.take(resource);
    }

    if (oldController)
    {
        emit controllerChanged(resource);
        oldController.reset(nullptr, &qnDeleteLater);
    }
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
    QnPtzControllerPtr oldController;
    {
        QnMutexLocker locker( &mutex );
        oldController = controllerByResource.value(resource);
    }

    if(oldController == controller)
        return;

    emit q->controllerAboutToBeChanged(resource);

    {
        QnMutexLocker locker(&mutex);
        controllerByResource.insert(resource, controller);
    }

    /* Some controller require an event loop to function, so we move them
     * to executor thread. Note that controllers don't run synchronous requests
     * in their associated thread, so this won't present any problems for
     * other users of the executor thread. */
    moveControllerToThread(controller, executorThread);

    emit q->controllerChanged(resource);

    if (oldController)
        oldController.reset(nullptr, &qnDeleteLater);
}

