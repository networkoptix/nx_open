#include "resource_pool.h"

#include <algorithm>

#include <QtCore/QMetaObject>

#include <core/resource/media_server_resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/videowall_item.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_matrix_index.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>


#ifdef QN_RESOURCE_POOL_DEBUG
#   define TRACE(...) qDebug << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

QnResourcePool::QnResourcePool() : QObject(),
    m_resourcesMtx(QMutex::Recursive),
    m_tranInProgress(false)
{}

QnResourcePool::~QnResourcePool()
{
    bool signalsBlocked = blockSignals(false);
    //emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    QMutexLocker locker(&m_resourcesMtx);
    m_resources.clear();
}

//Q_GLOBAL_STATIC(QnResourcePool, globalResourcePool)

static QnResourcePool* resourcePool_instance = NULL;

void QnResourcePool::initStaticInstance( QnResourcePool* inst )
{
    resourcePool_instance = inst;
}

QnResourcePool* QnResourcePool::instance()
{
    //return globalResourcePool();
    return resourcePool_instance;
}

void QnResourcePool::beginTran()
{
    m_tranInProgress = true;
}

void QnResourcePool::commit()
{
    m_tranInProgress = false;
    addResources(m_tmpResources);
    m_tmpResources.clear();
}

void QnResourcePool::addResource(const QnResourcePtr &resource)
{
    if (m_tranInProgress)
        m_tmpResources << resource;
    else
        addResources(QnResourceList() << resource); 
}

void QnResourcePool::addResources(const QnResourceList &resources)
{
    m_resourcesMtx.lock();

    foreach (const QnResourcePtr &resource, resources)
    {
        assert(resource->toSharedPointer()); /* Getting an assert here? Did you forget to use QnSharedResourcePointer? */

        if(resource->resourcePool() != NULL)
            qnWarning("Given resource '%1' is already in the pool.", resource->metaObject()->className());
            
        if (resource->getId().isNull())
        {
            // must be just found local resource; => shold not be in the pool already

            if (QnResourcePtr existing = getResourceByUniqId(resource->getUniqueId()))
            {
                // qnWarning("Resource with UID '%1' is already in the pool. Expect troubles.", resource->getUniqueId()); // TODO

                resource->setId(existing->getId());
            }
            else
            {
                resource->setId(QUuid::createUuid());
            }
        }

        resource->setResourcePool(this);
    }

    QMap<QUuid, QnResourcePtr> newResources; // sort by id

    foreach (const QnResourcePtr &resource, resources)
    {
        if( insertOrUpdateResource(
                resource,
                &m_resources ) )
        {
            newResources.insert(resource->getId(), resource);
        }
    }

    m_resourcesMtx.unlock();

    QnResourceList layouts;
    QnResourceList otherResources;

    foreach (const QnResourcePtr &resource, newResources.values())
    {
        connect(resource.data(), SIGNAL(statusChanged(const QnResourcePtr &)),      this, SIGNAL(statusChanged(const QnResourcePtr &)),     Qt::QueuedConnection);
        connect(resource.data(), SIGNAL(statusChanged(const QnResourcePtr &)),      this, SIGNAL(resourceChanged(const QnResourcePtr &)),   Qt::QueuedConnection);
        connect(resource.data(), SIGNAL(resourceChanged(const QnResourcePtr &)),    this, SIGNAL(resourceChanged(const QnResourcePtr &)),   Qt::QueuedConnection);

        if (!resource->hasFlags(Qn::foreigner))
        {
            if (resource->getStatus() != Qn::Offline)
                resource->initAsync(false);
        }

        if (resource.dynamicCast<QnLayoutResource>())
            layouts << resource;
        else
            otherResources << resource;
    }

    // Layouts should be notified first because their children should be instantiated before
    // 'UserChanged' event occurs
    foreach (const QnResourcePtr &resource, layouts) {
        TRACE("RESOURCE ADDED" << resource->metaObject()->className() << resource->getName());
        emit resourceAdded(resource);
    }

    foreach (const QnResourcePtr &resource, otherResources) {
        TRACE("RESOURCE ADDED" << resource->metaObject()->className() << resource->getName());
        emit resourceAdded(resource);
    }


}

namespace
{
    class MatchResourceByID
    {
    public:
        MatchResourceByID( const QUuid& _idToFind )
        :
            idToFind( _idToFind )
        {
        }

        bool operator()( const QnResourcePtr& res ) const
        {
            return res->getId() == idToFind;
        }

    private:
        QUuid idToFind;
    };
}

void QnResourcePool::removeResources(const QnResourceList &resources)
{
    QnResourceList removedResources;

    m_resourcesMtx.lock();

    foreach (const QnResourcePtr &resource, resources)
    {
        if (!resource)
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

        resource->setResourcePool(NULL);
    }

    /* Remove resources. */
    foreach (const QnResourcePtr &resource, removedResources) {
        disconnect(resource.data(), NULL, this, NULL);

        foreach(const QnLayoutResourcePtr &layoutResource, getResources().filtered<QnLayoutResource>()) // TODO: #Elric this is way beyond what one may call 'suboptimal'.
            foreach(const QnLayoutItemData &data, layoutResource->getItems())
                if(data.resource.id == resource->getId() || data.resource.path == resource->getUniqueId())
                    layoutResource->removeItem(data);

        if (resource.dynamicCast<QnLayoutResource>()) {
            foreach (const QnVideoWallResourcePtr &videowall, getResources().filtered<QnVideoWallResource>()) { // TODO: #Elric this is way beyond what one may call 'suboptimal'.
                foreach (QnVideoWallItem item, videowall->items()->getItems()) {
                    if (item.layout != resource->getId())
                        continue;
                    item.layout = QUuid();
                    videowall->items()->updateItem(item.uuid, item);
                }
            }
        }

        TRACE("RESOURCE REMOVED" << resource->metaObject()->className() << resource->getName());
    }

    m_resourcesMtx.unlock();

    /* Remove resources. */
    foreach (const QnResourcePtr &resource, removedResources) {
        emit resourceRemoved(resource);
    }
}

QnResourceList QnResourcePool::getResources() const
{
    QMutexLocker locker(&m_resourcesMtx);
    return m_resources.values();
}

QnResourcePtr QnResourcePool::getResourceById(const QUuid &id) const {
    QMutexLocker locker(&m_resourcesMtx);

    QHash<QString, QnResourcePtr>::const_iterator resIter = std::find_if( m_resources.begin(), m_resources.end(), MatchResourceByID(id) );
    if( resIter != m_resources.end() )
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

QnResourcePtr QnResourcePool::getResourceByParam(const QString &key, const QString &value) const
{
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) 
    {
        QVariant result;
        resource->getParam(key, result, QnDomainMemory);
        if (result.toString() == value)
            return resource;
    }

    return QnResourcePtr(0);
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

QnResourceList QnResourcePool::getAllCameras(const QnResourcePtr &mServer) const 
{
    QUuid parentId = mServer ? mServer->getId() : QUuid();
    QnResourceList result;
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) 
    {
        QnSecurityCamResourcePtr camResource = resource.dynamicCast<QnSecurityCamResource>();
        if (camResource && (parentId.isNull() || camResource->getParentId() == parentId))
            result << camResource;
    }

    return result;
}

QnMediaServerResourceList QnResourcePool::getAllServers() const 
{
    QMutexLocker locker(&m_resourcesMtx);
    QnMediaServerResourceList result;
    foreach (const QnResourcePtr &resource, m_resources) 
    {
        QnMediaServerResourcePtr mServer = resource.dynamicCast<QnMediaServerResource>();
        if (mServer)
            result << mServer;
    }

    return result;
}

QnResourceList QnResourcePool::getResourcesByParentId(const QUuid& parentId) const
{
    QnResourceList result;
    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) 
        if (resource->getParentId() == parentId)
            result << resource;

    return result;
}

QnResourceList QnResourcePool::getAllResourceByTypeName(const QString &typeName) const
{
    QnResourceList result;
    
    const QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName(typeName);
    if (!resType)
        return result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        if (resource->getTypeId() == resType->getId())
            result << resource;
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

QnResourcePtr QnResourcePool::getResourceByUniqId(const QString &id) const
{
    QMutexLocker locker(&m_resourcesMtx);
    QHash<QString, QnResourcePtr>::const_iterator itr = m_resources.find(id);
    return itr != m_resources.end() ? itr.value() : QnResourcePtr(0);
}

void QnResourcePool::updateUniqId(const QnResourcePtr& res, const QString &newUniqId)
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

QnResourceList QnResourcePool::getResourcesWithFlag(Qn::ResourceFlag flag) const
{
    QnResourceList result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources)
        if (resource->hasFlags(flag))
            result.append(resource);

    return result;
}

QnResourceList QnResourcePool::getResourcesWithParentId(QUuid id) const
{
    QMutexLocker locker(&m_resourcesMtx);

    // TODO: #Elric cache it, but remember that id and parentId of a resource may change
    // while it's in the pool.

    QnResourceList result;
    foreach(const QnResourcePtr &resource, m_resources)
        if(resource->getParentId() == id)
            result.push_back(resource);
    return result;
}

QnResourceList QnResourcePool::getResourcesWithTypeId(QUuid id) const
{
    QMutexLocker locker(&m_resourcesMtx);

    QnResourceList result;
    foreach(const QnResourcePtr &resource, m_resources)
        if(resource->getTypeId() == id)
            result.push_back(resource);
    return result;
}

QnUserResourcePtr QnResourcePool::getAdministrator() const {
    QMutexLocker locker(&m_resourcesMtx);

    foreach(const QnResourcePtr &resource, m_resources) {
        QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
        if (user && user->isAdmin())
            return user;
    }
    return QnUserResourcePtr();
}

QStringList QnResourcePool::allTags() const
{
    QStringList result;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources.values())
        result << resource->getTags();

    return result;
}

int QnResourcePool::activeCamerasByLicenseType(Qn::LicenseType licenseType) const
{
    int count = 0;

    QMutexLocker locker(&m_resourcesMtx);
    foreach (const QnResourcePtr &resource, m_resources) {
        QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (camera && !camera->isScheduleDisabled()) 
        {
            QnMediaServerResourcePtr mServer = getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();
            if (mServer && mServer->getStatus() != Qn::Offline)
            {
                if (camera->licenseType() == licenseType)
                    count++;
            }
        }
    }
    return count;
}

void QnResourcePool::clear()
{
    QMutexLocker lk( &m_resourcesMtx );

    m_resources.clear();
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

QnVideoWallItemIndex QnResourcePool::getVideoWallItemByUuid(const QUuid &uuid) const {
    foreach (const QnResourcePtr &resource, m_resources) {
        QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
        if (!videoWall || !videoWall->items()->hasItem(uuid))
            continue;
        return QnVideoWallItemIndex(videoWall, uuid);
    }
    return QnVideoWallItemIndex();
}

QnVideoWallItemIndexList QnResourcePool::getVideoWallItemsByUuid(const QList<QUuid> &uuids) const {
    QnVideoWallItemIndexList result;
    foreach (const QUuid &uuid, uuids) {
        QnVideoWallItemIndex index = getVideoWallItemByUuid(uuid);
        if (!index.isNull())
            result << index;
    }
    return result;
}

QnVideoWallMatrixIndex QnResourcePool::getVideoWallMatrixByUuid(const QUuid &uuid) const {
    foreach (const QnResourcePtr &resource, m_resources) {
        QnVideoWallResourcePtr videoWall = resource.dynamicCast<QnVideoWallResource>();
        if (!videoWall || !videoWall->matrices()->hasItem(uuid))
            continue;
        return QnVideoWallMatrixIndex(videoWall, uuid);
    }
    return QnVideoWallMatrixIndex();
}

QnVideoWallMatrixIndexList QnResourcePool::getVideoWallMatricesByUuid(const QList<QUuid> &uuids) const {
    QnVideoWallMatrixIndexList result;
    foreach (const QUuid &uuid, uuids) {
        QnVideoWallMatrixIndex index = getVideoWallMatrixByUuid(uuid);
        if (!index.isNull())
            result << index;
    }
    return result;
}
