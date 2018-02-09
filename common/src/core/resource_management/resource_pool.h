#pragma once

#include <functional>

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <common/common_globals.h>
#include <common/common_module_aware.h>

#include <utils/common/connective.h>

#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>

/**
 * This class holds all resources in the system that are READY TO BE USED
 * (as long as resource is in the pool => shared pointer counter >= 1).
 *
 * If resource is in the pool it is guaranteed that it won't be deleted.
 *
 * Resource pool can also give a list of resources based on some criteria
 * and helps to administrate resources.
 *
 */
class QN_EXPORT QnResourcePool: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    enum Filter
    {
        /** Do not check resources owned by another entities. */
        OnlyFriends,
        /** Check all resources. */
        AllResources
    };

    explicit QnResourcePool(QObject* parent = nullptr);
    ~QnResourcePool();

    enum AddResourceFlag
    {
        NoAddResourceFlags = 0x00,
        UseIncompatibleServerPool = 0x01,   //< TODO: #GDM Remove when implement VMS-6789
        SkipAddingTransaction = 0x02,       //< Add resource instantly even if transaction is open.
    };
    Q_DECLARE_FLAGS(AddResourceFlags, AddResourceFlag);

    /**
     * This function will add or update existing resources.
     * By default it add resources to the mainPool. if mainPoos parameter is false then resources are put to
     * the incompatible resource pool. Adding transaction is always skipped here.
     */
    void addResources(const QnResourceList &resources, AddResourceFlags flags = NoAddResourceFlags);

    /**
     * Add only those resources which are not belong to any resource pool. Existing resources are
     * not updated.
     */
    void addNewResources(const QnResourceList &resources,
        AddResourceFlags flags = NoAddResourceFlags);

    void addResource(const QnResourcePtr& resource, AddResourceFlags flags = NoAddResourceFlags);

    // TODO: We need to remove this function. Client should use separate instance of resource pool instead
    void addIncompatibleServer(const QnMediaServerResourcePtr& server);

    void beginTran();
    void commit();

    void removeResources(const QnResourceList &resources);
    inline void removeResource(const QnResourcePtr &resource)
    { removeResources(QnResourceList() << resource); }

    QnResourceList getResources() const;

    template <class IDList>
    QnResourceList getResources(IDList idList) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        QnResourceList result;
        for (const auto& id : idList)
        {
            const auto itr = m_resources.find(id);
            if (itr != m_resources.end())
                result.push_back(itr.value());
        }
        return result;
    }

    template <class Resource>
    QnSharedResourcePointerList<Resource> getResources() const
    {
        QnMutexLocker locker( &m_resourcesMtx );
        QnSharedResourcePointerList<Resource> result;
        for (const QnResourcePtr &resource : m_resources)
            if(QnSharedResourcePointer<Resource> derived = resource.template dynamicCast<Resource>())
                result.push_back(derived);
        return result;
    }

    template <class Resource, class IDList>
    QnSharedResourcePointerList<Resource> getResources(const IDList& idList) const
    {
        QnMutexLocker locker( &m_resourcesMtx );
        QnSharedResourcePointerList<Resource> result;
        for (const auto& id: idList)
        {
            const auto itr = m_resources.find(id);
            if (itr != m_resources.end()) {
                if (QnSharedResourcePointer<Resource> derived = itr.value().template dynamicCast<Resource>())
                    result.push_back(derived);
            }
        }
        return result;
    }

    QnResourcePtr getResource(std::function<bool(const QnResourcePtr&)> filter) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        auto itr = std::find_if( m_resources.begin(), m_resources.end(), filter);
        return itr != m_resources.end() ? itr.value() : QnResourcePtr();
    }

    template <class Resource>
    QnSharedResourcePointer<Resource> getResourceByUniqueId(const QString &id) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        auto itr = std::find_if( m_resources.begin(), m_resources.end(), [&id](const QnResourcePtr &resource) { return resource->getUniqueId() == id; });
        return itr != m_resources.end() ? itr.value().template dynamicCast<Resource>() : QnSharedResourcePointer<Resource>(NULL);
    }

    template <class Resource>
    QnSharedResourcePointer<Resource> getResourceById(const QnUuid &id) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        auto itr = m_resources.find(id);
        return itr != m_resources.end() ? itr.value().template dynamicCast<Resource>() : QnSharedResourcePointer<Resource>(NULL);
    }

    QnResourcePtr getResourceById(const QnUuid &id) const;

    // Shared id is groupId for multichannel resources and uniqueId for single channel resources.
    QnSecurityCamResourceList getResourcesBySharedId(const QString& sharedId) const;

    QnResourcePtr getResourceByUniqueId(const QString& uniqueId) const;

    QnResourcePtr getResourceByDescriptor(const QnLayoutItemResourceDescriptor& descriptor) const;

    QnResourcePtr getResourceByUrl(const QString &url) const;

    QnNetworkResourcePtr getNetResourceByPhysicalId(const QString &physicalId) const;
    QnNetworkResourcePtr getResourceByMacAddress(const QString &mac) const;

    QnNetworkResourceList getAllNetResourceByHostAddress(const QString &hostAddress) const;

    QnVirtualCameraResourceList getAllCameras(const QnResourcePtr &mServer = QnResourcePtr(),
        bool ignoreDesktopCameras = false) const;

    // @note Never returns fake servers
    QnMediaServerResourceList getAllServers(Qn::ResourceStatus status) const;

    QnResourceList getResourcesByParentId(const QnUuid& parentId) const;

    // returns list of resources with such flag
    QnResourceList getResourcesWithFlag(Qn::ResourceFlag flag) const;

    QnMediaServerResourcePtr getIncompatibleServerById(const QnUuid& id,
        bool useCompatible = false) const;

    QnMediaServerResourceList getIncompatibleServers() const;


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

    //!Empties all internal dictionaries. Needed for correct destruction order at application stop
    void clear();

    /** Check if there is at least one I/O Module in the system. */
    bool containsIoModules() const;

    /** In the future we may want to distinguish lite client layout from the others. */
    static void markLayoutLiteClient(const QnLayoutResourcePtr &layout);


    QnLayoutResourceList getLayoutsWithResource(const QnUuid &cameraGuid) const;

signals:
    void resourceAdded(const QnResourcePtr &resource);
    void resourceRemoved(const QnResourcePtr &resource);
    void resourceChanged(const QnResourcePtr &resource);
    void statusChanged(const QnResourcePtr &resource, Qn::StatusChangeReason reason);

    void aboutToBeDestroyed();

private:
    mutable struct Cache
    {
        void resourceRemoved(const QnResourcePtr& res);
        void resourceAdded(const QnResourcePtr& res);

        int ioModulesCount = 0;
        QMap<QnUuid, QnMediaServerResourcePtr> mediaServers;
        QMap<QString, QnResourcePtr> resourcesByUniqueId;
    private:
        bool isIoModule(const QnResourcePtr& res) const;
    } m_cache;

    mutable QnMutex m_resourcesMtx;
    bool m_tranInProgress;

    QnResourceList m_tmpResources;
    QHash<QnUuid, QnResourcePtr> m_resources;
    QHash<QnUuid, QnMediaServerResourcePtr> m_incompatibleServers;
    mutable QnUserResourcePtr m_adminResource;
};
