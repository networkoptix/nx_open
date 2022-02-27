// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

#include <utils/common/delayed.h>

namespace {

void moveControllerToThread(QnPtzControllerPtr controller, QThread* thread)
{
    QnPtzControllerPtr controllerIt = controller;
    while (controllerIt)
    {
        controllerIt->moveToThread(thread);
        QnProxyPtzControllerPtr proxyController = controllerIt.dynamicCast<QnProxyPtzController>();
        if (proxyController)
            controllerIt = proxyController->baseController();
        else
            controllerIt.clear();
    }
}

} // namespace

//-------------------------------------------------------------------------------------------------

class QnPtzControllerPoolPrivate
{
public:
    QnPtzControllerPoolPrivate():
        mode(QnPtzControllerPool::NormalControllerConstruction),
        executorThread(NULL),
        commandThreadPool(NULL),
        deinitialized(false)
    {
    }

    void updateController(const QnResourcePtr& resource);

    mutable nx::Mutex mutex;
    QAtomicInt mode;
    QHash<QnResourcePtr, QnPtzControllerPtr> controllerByResource;
    QThread *executorThread;
    QThreadPool *commandThreadPool;
    QnPtzControllerPool *q;
    std::atomic<bool> deinitialized;
};

//-------------------------------------------------------------------------------------------------

class QnPtzControllerCreationCommand: public QRunnable
{
public:
    QnPtzControllerCreationCommand(const QnResourcePtr& resource, QnPtzControllerPool* pool):
        m_resource(resource),
        m_pool(pool)
    {
    }

    virtual void run() override { m_pool->d->updateController(m_resource); }

private:
    QnResourcePtr m_resource;
    QnPtzControllerPool *m_pool;
};

//-------------------------------------------------------------------------------------------------
// QnPtzControllerPool methods

QnPtzControllerPool::QnPtzControllerPool(QObject* parent):
    base_type(parent),
    QnCommonModuleAware(parent),
    d(new QnPtzControllerPoolPrivate())
{
    d->q = this;

    d->executorThread = new QThread(this);
    d->executorThread->setObjectName("PTZExecutorThread");
    d->executorThread->start();

    d->commandThreadPool = new QThreadPool(this);
#ifdef __arm__
    const int maxThreads = 8;
#else
    const int maxThreads = 32;
#endif
    d->commandThreadPool->setMaxThreadCount(maxThreads);
}

QnPtzControllerPool::~QnPtzControllerPool()
{
    deinitialize();
}

void QnPtzControllerPool::init()
{
    connect(commonModule()->resourcePool(),
        &QnResourcePool::resourceAdded,
        this,
        &QnPtzControllerPool::registerResource);
    connect(commonModule()->resourcePool(),
        &QnResourcePool::resourceRemoved,
        this,
        &QnPtzControllerPool::unregisterResource);

    for (const QnResourcePtr& resource: commonModule()->resourcePool()->getResources())
        registerResource(resource);
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

        // Have to wait until all posted events are processed, deleteLater() can be called
        // within the event slot, that's why we specify the second parameter.
        WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed(d->executorThread, 3);
        waitingForObjectsToBeFreed.join();

        d->executorThread->exit();
        d->executorThread->wait();

        d->commandThreadPool->clear();
        d->commandThreadPool->waitForDone();

        d->deinitialized = true;
    }
}

QThread* QnPtzControllerPool::executorThread() const
{
    return d->executorThread;
}

QThreadPool* QnPtzControllerPool::commandThreadPool() const
{
    return d->commandThreadPool;
}

QnPtzControllerPool::ControllerConstructionMode QnPtzControllerPool::constructionMode() const
{
    return static_cast<ControllerConstructionMode>(d->mode.loadAcquire());
}

void QnPtzControllerPool::setConstructionMode(ControllerConstructionMode mode)
{
    d->mode.storeRelease(mode);
}

QnPtzControllerPtr QnPtzControllerPool::controller(const QnResourcePtr& resource) const
{
    NX_MUTEX_LOCKER locker(&d->mutex);
    return d->controllerByResource.value(resource);
}

QnPtzControllerPtr QnPtzControllerPool::createController(const QnResourcePtr& /*resource*/) const
{
    return QnPtzControllerPtr();
}

void QnPtzControllerPool::registerResource(const QnResourcePtr& resource)
{
    updateController(resource);
}

void QnPtzControllerPool::unregisterResource(const QnResourcePtr& resource)
{
    QnPtzControllerPtr oldController;

    {
        NX_MUTEX_LOCKER locker(&d->mutex);
        oldController = d->controllerByResource.take(resource);
    }

    if (oldController)
        emit controllerChanged(resource);
}

void QnPtzControllerPool::updateController(const QnResourcePtr& resource)
{
    if (d->mode.loadAcquire() == NormalControllerConstruction)
    {
        d->updateController(resource);
    }
    else
    {
        QRunnable *command = new QnPtzControllerCreationCommand(resource, this);
        command->setAutoDelete(true);
        d->commandThreadPool->start(command);
    }
}

void QnPtzControllerPoolPrivate::updateController(const QnResourcePtr& resource)
{
    QnPtzControllerPtr controller = q->createController(resource);
    QnPtzControllerPtr oldController;
    {
        NX_MUTEX_LOCKER locker(&mutex);
        oldController = controllerByResource.value(resource);
    }

    if (oldController == controller)
        return;

    emit q->controllerAboutToBeChanged(resource);

    {
        NX_MUTEX_LOCKER locker(&mutex);
        controllerByResource.insert(resource, controller);
    }

    /**
     * Some controllers require an event loop to function, so we move them to executor thread.
     * Note that controllers don't run synchronous requests in their associated thread, so this
     * won't make any problems for other users of the executor thread.
     */
    moveControllerToThread(controller, executorThread);
    auto weakRef = controller.toWeakRef();
    executeDelayed(
        [weakRef]()
        {
            if (auto controller = weakRef.lock())
                controller->initialize();
        },
        kDefaultDelay, executorThread);

    emit q->controllerChanged(resource);
}
