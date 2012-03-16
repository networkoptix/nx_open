#include "resource_pool.h"

#include <QtCore/QMetaObject>

#include "utils/common/warnings.h"
#include "utils/common/checked_cast.h"
#include "core/resource/video_server.h"
#include "core/resource/layout_resource.h"
#include "core/resource/user_resource.h"
#include "core/resource/camera_resource.h"

#ifdef QN_RESOURCE_POOL_DEBUG
#   define TRACE(...) qDebug << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

Q_GLOBAL_STATIC(QnResourcePool, globalResourcePool)

QnResourcePool::QnResourcePool() : QObject(),
    m_resourcesMtx(QMutex::Recursive)
{
    qRegisterMetaType<QnResourcePtr>("QnResourcePtr");
    qRegisterMetaType<QnResourceList>("QnResourceList");
    qRegisterMetaType<QnResource::Status>("QnResource::Status");

    localServer = QnResourcePtr(new QnLocalVideoServerResource);
    addResource(localServer);
}

QnResourcePool::~QnResourcePool()
{
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    QMutexLocker locker(&m_resourcesMtx);
    m_resources.clear();
}

QnResourcePool *QnResourcePool::instance()
{
    return globalResourcePool();
}

void QnResourcePool::addResources(const QnResourceList &resources)
{
    QMutexLocker locker(&m_resourcesMtx);

    foreach (const QnResourcePtr &resource, resources)
    {
        if (!resource->toSharedPointer())
        {
            qnWarning("Resource '%1' does not have an associated shared pointer. Did you forget to use QnSharedResourcePointer?", resource->metaObject()->className());
            QnSharedResourcePointer<QnResource>::initialize(resource);
        }

        if(resource->resourcePool() != NULL)
            qnWarning("Given resource '%1' is already in the pool.", resource->metaObject()->className());
            

        // if resources are local assign localserver as parent
        if (!resource->getParentId().isValid())
        {
            if (resource->checkFlags(QnResource::local))
                resource->setParentId(localServer->getId());
        }

        if (!resource->getId().isValid())
        {
            // must be just found local resource; => shold not be in the pool already

            if (QnResourcePtr existing = getResourceByUniqId(resource->getUniqueId()))
            {
                // qnWarning("Resource with UID '%1' is already in the pool. Expect troubles.", resource->getUniqueId()); // TODO

                resource->setId(existing->getId());
            }
            else
            {
                resource->setId(QnId::generateSpecialId());
            }
        }

        resource->setResourcePool(this);
    }

    QMap<QnId, QnResourcePtr> newResources; // sort by id

    foreach (const QnResourcePtr &resource, resources)
    {
        const QnId resId = resource->getId();
        QString uniqueId = resource->getUniqueId();

        if (m_resources.contains(uniqueId))
        {
            // if we already have such resource in the pool
            m_resources[uniqueId]->update(resource);
        }
        else
        {
            // new resource
            m_resources[uniqueId] = resource;
            newResources.insert(resId, resource);
        }
    }

    foreach (const QnResourcePtr &resource, newResources.values())
    {
        connect(resource.data(), SIGNAL(statusChanged(QnResource::Status,QnResource::Status)), this, SLOT(handleStatusChange()), Qt::QueuedConnection);
        connect(resource.data(), SIGNAL(statusChanged(QnResource::Status,QnResource::Status)), this, SLOT(handleResourceChange()), Qt::QueuedConnection);
		connect(resource.data(), SIGNAL(disabledChanged()), this, SLOT(handleResourceChange()), Qt::QueuedConnection);
        connect(resource.data(), SIGNAL(resourceChanged()), this, SLOT(handleResourceChange()), Qt::QueuedConnection);

        TRACE("RESOURCE ADDED" << resource->metaObject()->className() << resource->getName());
        emit resourceAdded(resource);
    }
}

void QnResourcePool::removeResources(const QnResourceList &resources)
{
    QnResourceList removedResources;

    QMutexLocker locker(&m_resourcesMtx);

    foreach (const QnResourcePtr &resource, resources)
    {
        if (!resource)
            continue;

        if (resource == localServer) // special case
            continue;

        if(resource->resourcePool() != this)
            qnWarning("Given resource '%1' is not in the pool", resource->metaObject()->className());

        QString uniqueId = resource->getUniqueId();

        if (m_resources.remove(uniqueId) != 0)
        {
            removedResources.append(resource);
        }

        resource->setResourcePool(NULL);
    }

    /* Remove resources. */
    foreach (const QnResourcePtr &resource, removedResources) {
        disconnect(resource.data(), NULL, this, NULL);

        TRACE("RESOURCE REMOVED" << resource->metaObject()->className() << resource->getName());
        emit resourceRemoved(resource);
    }
}

void QnResourcePool::handleStatusChange()
{
    const QnResourcePtr resource = toSharedPointer(checked_cast<QnResource *>(sender()));

    emit statusChanged(resource);
}

void QnResourcePool::handleResourceChange()
{
    const QnResourcePtr resource = toSharedPointer(checked_cast<QnResource *>(sender()));

    emit resourceChanged(resource);
}

QnResourceList QnResourcePool::getResources() const
{
    QMutexLocker locker(&m_resourcesMtx);
    return m_resources.values();
}

QnResourcePtr QnResourcePool::getResourceById(QnId id) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach(const QnResourcePtr& res, m_resources)
    {
        if (res->getId() == id)
            return res;
    }

    return QnResourcePtr(0);
}

QnResourcePtr QnResourcePool::getResourceByUrl(const QString &url) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        if (resource->getUrl() == url)
            return resource;
    }

    return QnResourcePtr(0);
}

QnNetworkResourcePtr QnResourcePool::getNetResourceByMac(const QString &mac) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        QnNetworkResourcePtr netResource = resource.dynamicCast<QnNetworkResource>();
        if (netResource != 0 && netResource->getMAC().toString() == mac)
            return netResource;
    }

    return QnNetworkResourcePtr(0);
}

QnResourcePtr QnResourcePool::getResourceByUniqId(const QString &id) const
{
    QMutexLocker locker(&m_resourcesMtx);
    QHash<QString, QnResourcePtr>::const_iterator itr = m_resources.find(id);
    return itr != m_resources.end() ? itr.value() : QnResourcePtr(0);
}

bool QnResourcePool::hasSuchResouce(const QString &uniqid) const
{
    return !getResourceByUniqId(uniqid).isNull();
}

QnResourceList QnResourcePool::getResourcesWithFlag(QnResource::Flag flag) const
{
    QnResourceList result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources)
        if (resource->checkFlags(flag))
            result.append(resource);

    return result;
}

QnResourceList QnResourcePool::getResourcesWithParentId(QnId id) const
{
    QMutexLocker locker(&m_resourcesMtx);

    // TODO:
    // cache it, but remember that id and parentId of a resource may change
    // while it's in the pool.

    QnResourceList result;
    foreach(const QnResourcePtr &resource, m_resources)
        if(resource->getParentId() == id)
            result.push_back(resource);
    return result;
}


QStringList QnResourcePool::allTags() const
{
    QStringList result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources.values())
        result << resource->tagList();

    return result;
}

QnResourcePtr QnResourcePool::getResourceByGuid(QString guid) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        if (resource->getGuid() == guid)
            return resource;
    }

    return QnNetworkResourcePtr(0);
}

int QnResourcePool::activeCameras() const
{
    int count = 0;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            continue;

		if (!camera->isDisabled() && !camera->isScheduleDisabled())
            count++;
    }

    return count;
}

