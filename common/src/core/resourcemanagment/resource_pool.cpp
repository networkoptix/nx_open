#include "resource_pool.h"

#include <QtCore/QMetaObject>

#include "utils/common/warnings.h"
#include "utils/common/checked_cast.h"
#include "core/resource/video_server.h"
#include "core/resource/layout_resource.h"
#include "core/resource/user_resource.h"
#include "core/resource/camera_resource.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"

#ifdef QN_RESOURCE_POOL_DEBUG
#   define TRACE(...) qDebug << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

Q_GLOBAL_STATIC(QnResourcePool, globalResourcePool)

QnResourcePool::QnResourcePool() : QObject(),
    m_resourcesMtx(QMutex::Recursive),
    m_updateLayouts(true)
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

bool QnResourcePool::isLayoutsUpdated() const {
    QMutexLocker locker(&m_resourcesMtx);

    return m_updateLayouts;
}

void QnResourcePool::setLayoutsUpdated(bool updateLayouts) {
    QMutexLocker locker(&m_resourcesMtx);

    m_updateLayouts = updateLayouts;
}

void QnResourcePool::addResources(const QnResourceList &resources)
{
    m_resourcesMtx.lock();

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

        /* Fix up items that are stored in layout. */
        if(QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>()) {
            foreach(QnLayoutItemData data, layout->getItems()) {
                if(!data.resource.id.isValid()) {
                    QnResourcePtr resource = qnResPool->getResourceByUniqId(data.resource.path);
                    if(!resource) {
                        if(data.resource.path.isEmpty()) {
                            qnWarning("Invalid item with empty id and path in layout '%1'.", layout->getName());
                        } else {
                            resource = QnResourcePtr(new QnAviResource(data.resource.path));
                            qnResPool->addResource(resource);
                        }
                    }

                    if(resource) {
                        data.resource.id = resource->getId();
                        layout->updateItem(data.uuid, data);
                    }
                } else if(data.resource.path.isEmpty()) {
                    QnResourcePtr resource = qnResPool->getResourceById(data.resource.id);
                    if(!resource) {
                        qnWarning("Invalid resource id '%1'.", data.resource.id.toString());
                    } else {
                        data.resource.path = resource->getUniqueId();
                        layout->updateItem(data.uuid, data);
                    }
                }
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

    m_resourcesMtx.unlock();

    foreach (const QnResourcePtr &resource, newResources.values())
    {
        connect(resource.data(), SIGNAL(statusChanged(QnResource::Status,QnResource::Status)), this, SLOT(handleStatusChange()), Qt::QueuedConnection);
        connect(resource.data(), SIGNAL(statusChanged(QnResource::Status,QnResource::Status)), this, SLOT(handleResourceChange()), Qt::QueuedConnection);
		connect(resource.data(), SIGNAL(disabledChanged(bool, bool)), this, SLOT(handleResourceChange()), Qt::QueuedConnection);
        connect(resource.data(), SIGNAL(resourceChanged()), this, SLOT(handleResourceChange()), Qt::QueuedConnection);

        if (resource->getStatus() != QnResource::Offline && !resource->isDisabled())
            resource->init();

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

        if(m_updateLayouts)
            foreach (const QnLayoutResourcePtr &layoutResource, QnResourceCriterion::filter<QnLayoutResource>(getResources())) // TODO: this is way beyond what one may call 'not optimal'.
                foreach(const QnLayoutItemData &data, layoutResource->getItems())
                    if(data.resource.id == resource->getId() || data.resource.path == resource->getUniqueId())
                        layoutResource->removeItem(data);

        TRACE("RESOURCE REMOVED" << resource->metaObject()->className() << resource->getName());

        emit resourceRemoved(resource);
    }
}

void QnResourcePool::handleStatusChange()
{
    const QnResourcePtr resource = toSharedPointer(checked_cast<QnResource *>(sender()));

    if (!resource)
        return;

    emit statusChanged(resource);
}

void QnResourcePool::handleResourceChange()
{
    const QnResourcePtr resource = toSharedPointer(checked_cast<QnResource *>(sender()));

    if (!resource)
        return;

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

QnNetworkResourcePtr QnResourcePool::getNetResourceByPhysicalId(const QString &physicalId) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        QnNetworkResourcePtr netResource = resource.dynamicCast<QnNetworkResource>();
        if (netResource != 0 && netResource->getPhysicalId() == physicalId)
            return netResource;
    }

    return QnNetworkResourcePtr(0);
}

QnNetworkResourceList QnResourcePool::getAllNetResourceByPhysicalId(const QString &physicalId) const
{
    QnNetworkResourceList result;
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        QnNetworkResourcePtr netResource = resource.dynamicCast<QnNetworkResource>();
        if (netResource != 0 && netResource->getPhysicalId() == physicalId)
            result << netResource;
    }

    return result;
}

QnNetworkResourcePtr QnResourcePool::getEnabledResourceByPhysicalId(const QString &physicalId) const {
    foreach(const QnNetworkResourcePtr &resource, getAllNetResourceByPhysicalId(physicalId))
        if(!resource->isDisabled())
            return resource;
    return QnNetworkResourcePtr();
}

QnResourcePtr QnResourcePool::getEnabledResourceByUniqueId(const QString &uniqueId) const {
    QnResourcePtr resource = getResourceByUniqId(uniqueId);
    if(!resource || !resource->isDisabled())
        return resource;

    QnNetworkResourcePtr networkResource = resource.dynamicCast<QnNetworkResource>();
    if(!networkResource)
        return QnResourcePtr();

    return getEnabledResourceByPhysicalId(networkResource->getPhysicalId());
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

