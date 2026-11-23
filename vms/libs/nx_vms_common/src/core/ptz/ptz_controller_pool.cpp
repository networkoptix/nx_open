// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ptz_controller_pool.h"

#include <set>

#include <QtCore/QThreadPool>

#include <core/ptz/ptz_ini.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/delayed.h>
#include <utils/common/waiting_for_qthread_to_empty_event_queue.h>

#include "abstract_ptz_controller.h"
#include "proxy_ptz_controller.h"

using namespace nx::vms::common;

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

struct QnPtzControllerPool::Private
{
    void updateControllerUntilUpToDate(const QnResourcePtr& resource);
    void updateController(const QnResourcePtr& resource);

    /** @return Whether the caller has to run the update, as none runs for the resource. */
    bool startUpdate(const QnResourcePtr& resource)
    {
        NX_MUTEX_LOCKER locker(&mutex);
        if (updatingResources.insert(resource).second)
            return true;
        outdatedResources.insert(resource);
        return false;
    }

    /** @return Whether the update has to be repeated, as another one was requested meanwhile. */
    bool finishUpdate(const QnResourcePtr& resource)
    {
        NX_MUTEX_LOCKER locker(&mutex);
        if (outdatedResources.erase(resource) > 0)
            return true;
        updatingResources.erase(resource);
        return false;
    }

    mutable nx::Mutex mutex;
    QHash<QnResourcePtr, QnPtzControllerPtr> controllerByResource;

    // Creations for one resource run one at a time, so the last one to finish reads the latest
    // device state. A request made while one runs makes it repeat once it finishes.
    std::set<QnResourcePtr> updatingResources;
    std::set<QnResourcePtr> outdatedResources; //< Requested again while being updated.

    QThread *executorThread = nullptr;
    QThreadPool *commandThreadPool = nullptr;
    QnPtzControllerPool *q;
    std::atomic<bool> stopped = false;
};

//-------------------------------------------------------------------------------------------------

// QnPtzControllerPool methods

QnPtzControllerPool::QnPtzControllerPool(
    QnResourcePool* resourcePool, ControllerConstructionMode mode, QObject* parent):
    QObject(parent),
    m_resourcePool(resourcePool),
    d(new Private()),
    m_mode(mode)
{
    d->q = this;

    d->executorThread = new QThread(this);
    d->executorThread->setObjectName("PTZExecutorThread");
    d->executorThread->start();

    d->commandThreadPool = new QThreadPool(this);
    d->commandThreadPool->setMaxThreadCount(ptzIni().maxPtzControllerPoolThreadCount);
}

QnPtzControllerPool::~QnPtzControllerPool()
{
    {
        NX_MUTEX_LOCKER locker(&d->mutex);
        d->controllerByResource.clear();
    }
    stop();
}

void QnPtzControllerPool::init()
{
    connect(m_resourcePool,
        &QnResourcePool::resourcesAdded,
        this,
        [this] (const QnResourceList& resources)
        {
            for (const auto& resource: resources)
                registerResource(resource);
        });
    connect(m_resourcePool,
        &QnResourcePool::resourcesRemoved,
        this,
        [this] (const QnResourceList& resources)
        {
            for (const auto& resource: resources)
                unregisterResource(resource);
        });

    for (const QnResourcePtr& resource: m_resourcePool->getResources())
        registerResource(resource);
}

void QnPtzControllerPool::pleaseStop()
{
    d->stopped = true;
}

bool QnPtzControllerPool::isStopping() const
{
    return d->stopped;
}

void QnPtzControllerPool::stop()
{
    pleaseStop();
    while (!d->controllerByResource.isEmpty())
    {
        auto resourcePtr = d->controllerByResource.begin().key();
        unregisterResource(resourcePtr);
    }

    // Have to wait until all posted events are processed, deleteLater() can be called
    // within the event slot, that's why we specify the second parameter.
    if (d->executorThread->isRunning())
    {
        WaitingForQThreadToEmptyEventQueue waitingForObjectsToBeFreed(d->executorThread, 3);
        waitingForObjectsToBeFreed.join();
        d->executorThread->exit();
        d->executorThread->wait();
    }

    d->commandThreadPool->clear();
    d->commandThreadPool->waitForDone();
}

QThread* QnPtzControllerPool::executorThread() const
{
    return d->executorThread;
}

QThreadPool* QnPtzControllerPool::commandThreadPool() const
{
    return d->commandThreadPool;
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
    if (!d->stopped)
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
    if (!d->startUpdate(resource))
        return; //< The running update repeats itself.

    if (m_mode == NormalControllerConstruction)
    {
        d->updateControllerUntilUpToDate(resource);
    }
    else
    {
        d->commandThreadPool->start(
            [this, resource] { d->updateControllerUntilUpToDate(resource); });
    }
}

void QnPtzControllerPool::Private::updateControllerUntilUpToDate(const QnResourcePtr& resource)
{
    do
        updateController(resource);
    while (finishUpdate(resource));
}

void QnPtzControllerPool::Private::updateController(const QnResourcePtr& resource)
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
