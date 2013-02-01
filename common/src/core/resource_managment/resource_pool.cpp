#include "resource_pool.h"

#include <QtCore/QMetaObject>

#include "utils/common/warnings.h"
#include "utils/common/checked_cast.h"
#include "common/common_meta_types.h"
#include "core/resource/media_server_resource.h"
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
    m_resourcesMtx(QMutex::Recursive),
    m_updateLayouts(true)
{
    QnCommonMetaTypes::initilize();

    localServer = QnResourcePtr(new QnLocalMediaServerResource);
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
            if (resource->hasFlags(QnResource::local))
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
        /*
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
        */

        resource->setResourcePool(this);
    }

    QMap<QnId, QnResourcePtr> newResources; // sort by id

    foreach (const QnResourcePtr &resource, resources)
    {
        if( insertOrUpdateResource(
                resource,
                resource->hasFlags(QnResource::foreigner) ? &m_foreignResources : &m_resources ) )
        {
            newResources.insert(resource->getId(), resource);
        }
    }

    m_resourcesMtx.unlock();

    foreach (const QnResourcePtr &resource, newResources.values())
    {
        connect(resource.data(), SIGNAL(statusChanged(const QnResourcePtr &)),      this, SIGNAL(statusChanged(const QnResourcePtr &)),     Qt::QueuedConnection);
        connect(resource.data(), SIGNAL(statusChanged(const QnResourcePtr &)),      this, SIGNAL(resourceChanged(const QnResourcePtr &)),   Qt::QueuedConnection);
        connect(resource.data(), SIGNAL(disabledChanged(const QnResourcePtr &)),    this, SIGNAL(resourceChanged(const QnResourcePtr &)),   Qt::QueuedConnection);
        connect(resource.data(), SIGNAL(resourceChanged(const QnResourcePtr &)),    this, SIGNAL(resourceChanged(const QnResourcePtr &)),   Qt::QueuedConnection);

        if (!resource->hasFlags(QnResource::foreigner))
        {
            if (resource->getStatus() != QnResource::Offline && !resource->isDisabled())
                resource->init();
        }

        TRACE("RESOURCE ADDED" << resource->metaObject()->className() << resource->getName());
        emit resourceAdded(resource);
    }
}

namespace
{
    class MatchResourceByID
    {
    public:
        MatchResourceByID( const QnId& _idToFind )
        :
            idToFind( _idToFind )
        {
        }

        bool operator()( const QnResourcePtr& res ) const
        {
            return res->getId() == idToFind;
        }

    private:
        QnId idToFind;
    };
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

        //const QString& uniqueId = resource->getUniqueId();
        //if( m_resources.remove(uniqueId) != 0 )
        //    removedResources.append(resource);
        
        //have to remove by id, since uniqueId can be MAC and, as a result, not unique among friend and foreign resources
        QHash<QString, QnResourcePtr>::iterator resIter = std::find_if( m_resources.begin(), m_resources.end(), MatchResourceByID(resource->getId()) );
        if( resIter != m_resources.end() )
        {
            m_resources.erase( resIter );
            removedResources.append(resource);
        }
        else
        {
            //may be foreign resource?
            resIter = std::find_if( m_foreignResources.begin(), m_foreignResources.end(), MatchResourceByID(resource->getId()) );
            if( resIter != m_foreignResources.end() )
            {
                m_foreignResources.erase( resIter );
                removedResources.append(resource);
            }
        }

        resource->setResourcePool(NULL);
    }

    /* Remove resources. */
    foreach (const QnResourcePtr &resource, removedResources) {
        disconnect(resource.data(), NULL, this, NULL);

        if(m_updateLayouts)
            foreach (const QnLayoutResourcePtr &layoutResource, getResources().filtered<QnLayoutResource>()) // TODO: this is way beyond what one may call 'suboptimal'.
                foreach(const QnLayoutItemData &data, layoutResource->getItems())
                    if(data.resource.id == resource->getId() || data.resource.path == resource->getUniqueId())
                        layoutResource->removeItem(data);

        TRACE("RESOURCE REMOVED" << resource->metaObject()->className() << resource->getName());

        emit resourceRemoved(resource);
    }
}

QnResourceList QnResourcePool::getResources() const
{
    QMutexLocker locker(&m_resourcesMtx);
    return m_resources.values();
}

QnResourcePtr QnResourcePool::getResourceById(QnId id, Filter searchFilter) const
{
    QMutexLocker locker(&m_resourcesMtx);

    QHash<QString, QnResourcePtr>::const_iterator resIter = std::find_if( m_resources.begin(), m_resources.end(), MatchResourceByID(id) );
    if( resIter != m_resources.end() )
        return resIter.value();

    if( searchFilter == rfOnlyFriends )
        return QnResourcePtr(NULL);

    //looking through foreign resources
    resIter = std::find_if( m_foreignResources.begin(), m_foreignResources.end(), MatchResourceByID(id) );
    if( resIter != m_foreignResources.end() )
        return resIter.value();

    return QnResourcePtr(NULL);
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

QnNetworkResourcePtr QnResourcePool::getResourceByMacAddress(const QString &mac) const
{
    QnMacAddress macAddress(mac);
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        QnNetworkResourcePtr netResource = resource.dynamicCast<QnNetworkResource>();
        if (netResource != 0 && netResource->getMAC() == macAddress)
            return netResource;
    }

    return QnNetworkResourcePtr(0);
}

QnResourceList QnResourcePool::getAllEnabledCameras() const
{
    QnResourceList result;
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        QnSecurityCamResourcePtr camResource = resource.dynamicCast<QnSecurityCamResource>();
        if (camResource != 0 && !camResource->isDisabled() && !camResource->hasFlags(QnResource::foreigner))
            result << camResource;
    }

    return result;
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

QnNetworkResourceList QnResourcePool::getAllNetResourceByHostAddress(const QHostAddress &hostAddress) const
{
    return getAllNetResourceByHostAddress(hostAddress.toString());
}

QnNetworkResourceList QnResourcePool::getAllNetResourceByHostAddress(const QString &hostAddress) const
{
    QnNetworkResourceList result;
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        QnNetworkResourcePtr netResource = resource.dynamicCast<QnNetworkResource>();
        if (netResource != 0 && netResource->getHostAddress() == hostAddress)
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

void QnResourcePool::updateUniqId(QnResourcePtr res, const QString &newUniqId)
{
    QMutexLocker locker(&m_resourcesMtx);
    QHash<QString, QnResourcePtr>::iterator itr = m_resources.find(res->getUniqueId());
    if (itr != m_resources.end())
        m_resources.erase(itr);
    res->setUniqId(newUniqId);
    m_resources.insert(newUniqId, res);
}

bool QnResourcePool::hasSuchResource(const QString &uniqid) const
{
    return !getResourceByUniqId(uniqid).isNull();
}

QnResourceList QnResourcePool::getResourcesWithFlag(QnResource::Flag flag) const
{
    QnResourceList result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources)
        if (resource->hasFlags(flag))
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

QnResourceList QnResourcePool::getResourcesWithTypeId(QnId id) const
{
    QMutexLocker locker(&m_resourcesMtx);

    QnResourceList result;
    foreach(const QnResourcePtr &resource, m_resources)
        if(resource->getTypeId() == id)
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

bool QnResourcePool::insertOrUpdateResource( const QnResourcePtr &resource, QHash<QString, QnResourcePtr>* const resourcePool )
{
    const QString& uniqueId = resource->getUniqueId();

    if (resourcePool->contains(uniqueId))
    {
        // if we already have such resource in the pool
        (*resourcePool)[uniqueId]->update(resource);
        return false;
    }
    else
    {
        // new resource
        (*resourcePool)[uniqueId] = resource;
        return true;
    }
}
