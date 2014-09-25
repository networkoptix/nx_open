#ifndef resource_pool_h_1537
#define resource_pool_h_1537

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <utils/common/uuid.h>
#include <QtNetwork/QHostAddress>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource.h>
#include <core/resource_management/resource_criterion.h>

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
class QN_EXPORT QnResourcePool : public QObject
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

    QnResourcePool();
    ~QnResourcePool();

    static void initStaticInstance( QnResourcePool* inst );
    static QnResourcePool* instance();

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

    template <class Resource>
    QnSharedResourcePointerList<Resource> getResources() const {
        QMutexLocker locker(&m_resourcesMtx);
        QnSharedResourcePointerList<Resource> result;
        for (const QnResourcePtr &resource : m_resources)
            if(QnSharedResourcePointer<Resource> derived = resource.template dynamicCast<Resource>())
                result.push_back(derived);
        return result;
    }

    QnResourcePtr getResourceById(const QnUuid &id) const;

    QnResourcePtr getResourceByUniqId(const QString &id) const;
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
    QnResourceList getAllCameras(const QnResourcePtr &mServer, bool ignoreDesktopCameras = false) const;
    QnMediaServerResourceList getAllServers() const;
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

    int activeCamerasByLicenseType(Qn::LicenseType licenseType) const;

    //!Empties all internal dictionaries. Needed for correct destruction order at application stop
    void clear();

signals:
    void resourceAdded(const QnResourcePtr &resource);
    void resourceRemoved(const QnResourcePtr &resource);
    void resourceChanged(const QnResourcePtr &resource);
    void statusChanged(const QnResourcePtr &resource);

    void aboutToBeDestroyed();

private:
    mutable QMutex m_resourcesMtx;
    bool m_tranInProgress;
    QnResourceList m_tmpResources;
    QHash<QnUuid, QnResourcePtr> m_resources;
    QHash<QnUuid, QnResourcePtr> m_incompatibleResources;

    /*!
        \return true, if \a resource has been inserted. false - if updated existing resource
    */
    bool insertOrUpdateResource( const QnResourcePtr &resource, QHash<QnUuid, QnResourcePtr>* const resourcePool );
};


#define qnResPool QnResourcePool::instance()

#endif //resource_pool_h_1537
