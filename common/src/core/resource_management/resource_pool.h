#ifndef resource_pool_h_1537
#define resource_pool_h_1537

#include <QtCore/QList>
#include <QtCore/QHash>
#include <utils/thread/mutex.h>
#include <QtCore/QObject>
#include <utils/common/uuid.h>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_criterion.h>

#include <utils/common/singleton.h>

class QnResource;
class QnNetworkResource;
class CLRecorderDevice;

/**
 * This class holds all resources in the system that are READY TO BE USED
 * (as long as resource is in the pool => shared pointer counter >= 1).
 *
 * If resource is in the pool it is guaranteed that it won't be deleted.
 *
 * Resource pool can also give a list of resources based on some criteria
 * and helps to administrate resources.
 *
 * If resource is conflicting it must not be placed in resource pool.
 */
class QN_EXPORT QnResourcePool : public QObject, public Singleton<QnResourcePool>
{
    Q_OBJECT

public:
    enum Filter
    {
        /** Do not check resources owned by another entities. */
        OnlyFriends,
        /** Check all resources. */
        AllResources
    };

    QnResourcePool(QObject* parent = NULL);
    ~QnResourcePool();

    // this function will add or update existing resources
    // keeps database ID ( if possible )
    void addResources(const QnResourceList &resources);

    void addResource(const QnResourcePtr &resource);

    void beginTran();
    void commit();

    void removeResources(const QnResourceList &resources);
    inline void removeResource(const QnResourcePtr &resource)
    { removeResources(QnResourceList() << resource); }

    QnResourceList getResources() const;
    QnResourceList getResources(const QVector<QnUuid>& idList) const;
    QnResourceList getResources(const std::vector<QnUuid>& idList) const;

    template <class Resource>
    QnSharedResourcePointerList<Resource> getResources() const {
        QnMutexLocker locker( &m_resourcesMtx );
        QnSharedResourcePointerList<Resource> result;
        for (const QnResourcePtr &resource : m_resources)
            if(QnSharedResourcePointer<Resource> derived = resource.template dynamicCast<Resource>())
                result.push_back(derived);
        return result;
    }

    template <class Resource>
    QnSharedResourcePointerList<Resource> getResources(const QVector<QnUuid>& idList) const {
        QnMutexLocker locker( &m_resourcesMtx );
        QnSharedResourcePointerList<Resource> result;
        for (const auto& id: idList) {
            const auto itr = m_resources.find(id);
            if (itr != m_resources.end()) {
                if(QnSharedResourcePointer<Resource> derived = itr.value().template dynamicCast<Resource>())
                    result.push_back(derived);
            }
        }
        return result;
    }

    template <class Resource>
    QnSharedResourcePointerList<Resource> getResources(const std::vector<QnUuid>& idList) const {
        QnMutexLocker locker(&m_resourcesMtx);
        QnSharedResourcePointerList<Resource> result;
        for (const auto& id: idList) {
            const auto itr = m_resources.find(id);
            if (itr != m_resources.end()) {
                if(QnSharedResourcePointer<Resource> derived = itr.value().template dynamicCast<Resource>())
                    result.push_back(derived);
            }
        }
        return result;
    }

    template <class Resource>
    QnSharedResourcePointer<Resource> getResourceByUniqueId(const QString &id) const {
        QnMutexLocker locker(&m_resourcesMtx);
        auto itr = std::find_if( m_resources.begin(), m_resources.end(), [&id](const QnResourcePtr &resource) { return resource->getUniqueId() == id; });
        return itr != m_resources.end() ? itr.value().template dynamicCast<Resource>() : QnSharedResourcePointer<Resource>(NULL);
    }

    template <class Resource>
    QnSharedResourcePointer<Resource> getResourceById(const QnUuid &id) const {
        QnMutexLocker locker(&m_resourcesMtx);
        auto itr = m_resources.find(id);
        return itr != m_resources.end() ? itr.value().template dynamicCast<Resource>() : QnSharedResourcePointer<Resource>(NULL);
    }

    QnResourcePtr getResourceById(const QnUuid &id) const;

    QnResourcePtr getResourceByUniqueId(const QString &id) const;
    void updateUniqId(const QnResourcePtr& res, const QString &newUniqId);

    bool hasSuchResource(const QString &uniqid) const;

    QnResourcePtr getResourceByUrl(const QString &url) const;

    QnNetworkResourcePtr getNetResourceByPhysicalId(const QString &physicalId) const;
    QnNetworkResourcePtr getResourceByMacAddress(const QString &mac) const;
    QnResourcePtr getResourceByParam(const QString &key, const QString &value) const;

    QnResourceList getAllResourceByTypeName(const QString &typeName) const;

    QnNetworkResourceList getAllNetResourceByPhysicalId(const QString &mac) const;
    QnNetworkResourceList getAllNetResourceByHostAddress(const QString &hostAddress) const;
    QnNetworkResourceList getAllNetResourceByHostAddress(const QHostAddress &hostAddress) const;
    QnVirtualCameraResourceList getAllCameras(const QnResourcePtr &mServer, bool ignoreDesktopCameras = false) const;

    // @note Never returns fake servers
    QnMediaServerResourceList getAllServers(Qn::ResourceStatus status) const;

    QnResourceList getResourcesByParentId(const QnUuid& parentId) const;

    // returns list of resources with such flag
    QnResourceList getResourcesWithFlag(Qn::ResourceFlag flag) const;

    QnResourceList getResourcesWithParentId(QnUuid id) const;
    QnResourceList getResourcesWithTypeId(QnUuid id) const;

    QnResourcePtr getIncompatibleResourceById(const QnUuid &id, bool useCompatible = false) const;
    QnResourcePtr getIncompatibleResourceByUniqueId(const QString &uid) const;
    QnResourceList getAllIncompatibleResources() const;

    QnUserResourcePtr getAdministrator() const;

    /**
     * @brief getVideoWallItemByUuid            Find videowall item by uuid.
     * @param uuid                              Unique id of the item.
     * @return                                  Valid index containing the videowall and item's uuid or null index if such item does not exist.
     */
    QnVideoWallItemIndex getVideoWallItemByUuid(const QnUuid &uuid) const;

    /**
     * @brief getVideoWallItemsByUuid           Find list of videowall items by their uuids.
     * @param uuids                             Unique ids of the items.
     * @return                                  List of valid indices containing the videowall and items' uuid.
     */
    QnVideoWallItemIndexList getVideoWallItemsByUuid(const QList<QnUuid> &uuids) const;

    /**
     * @brief getVideoWallMatrixByUuid          Find videowall matrix by uuid.
     * @param uuid                              Unique id of the matrix.
     * @return                                  Index containing the videowall and matrix's uuid.
     */
    QnVideoWallMatrixIndex getVideoWallMatrixByUuid(const QnUuid &uuid) const;

    /**
     * @brief getVideoWallMatricesByUuid        Find list of videowall matrices by their uuids.
     * @param uuids                             Unique ids of the matrices.
     * @return                                  List of indices containing the videowall and matrices' uuid.
     */
    QnVideoWallMatrixIndexList getVideoWallMatricesByUuid(const QList<QnUuid> &uuids) const;

    QStringList allTags() const;

    //!Empties all internal dictionaries. Needed for correct destruction order at application stop
    void clear();

    /** Check if there is at least one IO Module in the system. */
    bool containsIoModules() const;

    /** Check if layout was created automatically, e.g. by 'Push my screen' action.
     *  //TODO: #GDM replace this method by corresponding boolean flag in 2.6
     */
    static bool isAutoGeneratedLayout(const QnLayoutResourcePtr &layout);
    static void markLayoutAutoGenerated(const QnLayoutResourcePtr &layout);


    QnLayoutResourceList getLayoutsWithResource(const QnUuid &cameraGuid) const;
signals:
    void resourceAdded(const QnResourcePtr &resource);
    void resourceRemoved(const QnResourcePtr &resource);
    void resourceChanged(const QnResourcePtr &resource);
    void statusChanged(const QnResourcePtr &resource);

    void aboutToBeDestroyed();
private:
    /*!
        \note MUST be called with \a m_resourcesMtx locked
    */
    void invalidateCache();
    void ensureCache() const;

private:
    mutable struct Cache {
        bool valid;
        bool containsIoModules;
        QnMediaServerResourceList serversList;
    } m_cache;

    mutable QnMutex m_resourcesMtx;
    bool m_tranInProgress;

    QnResourceList m_tmpResources;
    QHash<QnUuid, QnResourcePtr> m_resources;
    QHash<QnUuid, QnResourcePtr> m_incompatibleResources;
    mutable QnUserResourcePtr m_adminResource;

    /*!
        \return true, if \a resource has been inserted. false - if updated existing resource
    */
    bool insertOrUpdateResource( const QnResourcePtr &resource, QHash<QnUuid, QnResourcePtr>* const resourcePool );
};


#define qnResPool QnResourcePool::instance()

#endif //resource_pool_h_1537
