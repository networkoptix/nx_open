#pragma once

#include <vector>
#include <functional>

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QObject>

#include <api/helpers/camera_id_helper.h>

#include <core/resource/resource_fwd.h>

#include <common/common_globals.h>
#include <common/common_module_aware.h>

#include <utils/common/connective.h>

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/impl_ptr.h>

/**
 * This class holds all resources in the system that are READY TO BE USED (as long as resource is
 * in the pool => shared pointer counter >= 1).
 *
 * If resource is in the pool it is guaranteed that it won't be deleted.
 *
 * Resource pool can also give a list of resources based on some criteria and helps to administrate
 * resources.
 */
class QnResourcePool: public Connective<QObject>, public /*mixin*/ QnCommonModuleAware
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
        UseIncompatibleServerPool = 0x01, //< TODO: #GDM Remove when implement VMS-6789
        SkipAddingTransaction = 0x02, //< Add resource instantly even if transaction is open.
    };
    Q_DECLARE_FLAGS(AddResourceFlags, AddResourceFlag);

    /**
     * This function will add or update existing resources.
     */
    void addResources(
        const QnResourceList& resources,
        AddResourceFlags flags = NoAddResourceFlags);

    /**
     * Add only those resources which are not belong to any resource pool. Existing resources are
     * not updated.
     */
    void addNewResources(
        const QnResourceList& resources,
        AddResourceFlags flags = NoAddResourceFlags);

    void addResource(const QnResourcePtr& resource, AddResourceFlags flags = NoAddResourceFlags);

    // TODO: We need to remove this function. Client should use separate instance of resource pool instead
    void addIncompatibleServer(const QnMediaServerResourcePtr& server);

    void beginTran();
    void commit();

    void removeResources(const QnResourceList& resources);

    void removeResource(const QnResourcePtr& resource)
    {
        removeResources(QnResourceList() << resource);
    }

    //!Empties all internal dictionaries. Needed for correct destruction order at application stop
    void clear();

    using ResourceFilter = std::function<bool(const QnResourcePtr&)>;

    template<class Resource>
    using ResourceClassFilter = std::function<bool(const QnSharedResourcePointer<Resource>&)>;

    //---------------------------------------------------------------------------------------------
    // Methods to get all resources.

    QnResourceList getResources() const;

    template<class Resource>
    QnSharedResourcePointerList<Resource> getResources() const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        QnSharedResourcePointerList<Resource> result;
        for (const QnResourcePtr& resource: m_resources)
            if (auto derived = resource.template dynamicCast<Resource>())
                result.push_back(derived);
        return result;
    }

    //---------------------------------------------------------------------------------------------
    // Methods to get resources by id.

    template<class IdList>
    QnResourceList getResourcesByIds(IdList idList) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        QnResourceList result;
        for (const auto& id: idList)
        {
            const auto itr = m_resources.find(id);
            if (itr != m_resources.end())
                result.push_back(itr.value());
        }
        return result;
    }

    QnVirtualCameraResourceList getCamerasByFlexibleIds(const std::vector<QString>& flexibleIdList) const;

    template<class Resource, class IdList>
    QnSharedResourcePointerList<Resource> getResourcesByIds(const IdList& idList) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        QnSharedResourcePointerList<Resource> result;
        for (const QnUuid& id: idList)
        {
            const auto itr = m_resources.find(id);
            if (itr != m_resources.end())
            {
                if (auto derived = itr.value().template dynamicCast<Resource>())
                    result.push_back(derived);
            }
        }
        return result;
    }

    //---------------------------------------------------------------------------------------------
    // Methods to get resources by custom filter.

    QnResourceList getResources(ResourceFilter filter) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        QnResourceList result;
        for (const QnResourcePtr& resource: m_resources)
        {
            if (filter(resource))
                result.push_back(resource);
        }
        return result;
    }

    template<class Resource>
    QnSharedResourcePointerList<Resource> getResources(ResourceClassFilter<Resource> filter) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        QnSharedResourcePointerList<Resource> result;
        for (const QnResourcePtr& resource: m_resources)
        {
            if (auto derived = resource.template dynamicCast<Resource>())
            {
                if (filter(derived))
                    result.push_back(derived);
            }
        }
        return result;
    }

    QnNetworkResourceList getAllNetResourceByHostAddress(const QString& hostAddress) const;

    QnVirtualCameraResourceList getAllCameras(
        const QnResourcePtr& mServer = QnResourcePtr(),
        bool ignoreDesktopCameras = false) const;

    // Never returns fake servers.
    QnMediaServerResourceList getAllServers(Qn::ResourceStatus status) const;

    QnResourceList getResourcesByParentId(const QnUuid& parentId) const;

    // Returns list of resources with such flag.
    QnResourceList getResourcesWithFlag(Qn::ResourceFlag flag) const;

    QnMediaServerResourceList getIncompatibleServers() const;

    //---------------------------------------------------------------------------------------------
    // Methods to get single resource by various conditions.

    QnResourcePtr getResource(ResourceFilter filter) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        auto itr = std::find_if(m_resources.begin(), m_resources.end(), filter);
        return itr != m_resources.end() ? itr.value() : QnResourcePtr();
    }

    template<class Resource>
    QnSharedResourcePointer<Resource> getResource(ResourceClassFilter<Resource> filter) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        for (const QnResourcePtr& resource: m_resources)
        {
            if (auto derived = resource.template dynamicCast<Resource>())
                if (filter(derived))
                    return derived;
        }
        return QnSharedResourcePointer<Resource>();
    }

    template<class Resource>
    QnSharedResourcePointer<Resource> getResourceByUniqueId(const QString& id) const
    {
        return getResource<Resource>(
            [&id](const QnSharedResourcePointer<Resource>& resource)
            {
                return resource->getUniqueId() == id;
            });
    }

    template<class Resource>
    QnSharedResourcePointer<Resource> getResourceById(const QnUuid& id) const
    {
        QnMutexLocker locker(&m_resourcesMtx);
        auto itr = m_resources.find(id);
        return itr != m_resources.end()
            ? itr.value().template dynamicCast<Resource>()
            : QnSharedResourcePointer<Resource>();
    }

    QnResourcePtr getResourceById(const QnUuid& id) const;

    // Shared id is groupId for multichannel resources and uniqueId for single channel resources.
    QnSecurityCamResourceList getResourcesBySharedId(const QString& sharedId) const;

    /**
     * Returns resource list by logicalId. logicaId is a some camera tag used for intergration
     * with external systems.
     */
    QnResourceList getResourcesByLogicalId(int value) const;

    QnResourcePtr getResourceByUniqueId(const QString& uniqueId) const;

    QnResourcePtr getResourceByDescriptor(const QnLayoutItemResourceDescriptor& descriptor) const;

    QnResourcePtr getResourceByUrl(const QString& url) const;

    QnNetworkResourcePtr getNetResourceByPhysicalId(const QString& physicalId) const;
    QnNetworkResourcePtr getResourceByMacAddress(const QString& mac) const;


    QnMediaServerResourcePtr getIncompatibleServerById(
        const QnUuid& id,
        bool useCompatible = false) const;

    QnUserResourcePtr getAdministrator() const;

    /**
     * @brief getVideoWallItemByUuid            Find videowall item by uuid.
     * @param uuid                              Unique id of the item.
     * @return                                  Valid index containing the videowall and item's uuid or null index if such item does not exist.
     */
    QnVideoWallItemIndex getVideoWallItemByUuid(const QnUuid& uuid) const;

    /**
     * @brief getVideoWallItemsByUuid           Find list of videowall items by their uuids.
     * @param uuids                             Unique ids of the items.
     * @return                                  List of valid indices containing the videowall and items' uuid.
     */
    QnVideoWallItemIndexList getVideoWallItemsByUuid(const QList<QnUuid>& uuids) const;

    /**
     * @brief getVideoWallMatrixByUuid          Find videowall matrix by uuid.
     * @param uuid                              Unique id of the matrix.
     * @return                                  Index containing the videowall and matrix's uuid.
     */
    QnVideoWallMatrixIndex getVideoWallMatrixByUuid(const QnUuid& uuid) const;

    /**
     * @brief getVideoWallMatricesByUuid        Find list of videowall matrices by their uuids.
     * @param uuids                             Unique ids of the matrices.
     * @return                                  List of indices containing the videowall and matrices' uuid.
     */
    QnVideoWallMatrixIndexList getVideoWallMatricesByUuid(const QList<QnUuid>& uuids) const;

    /** Check if there is at least one I/O Module in the system. */
    bool containsIoModules() const;

    /** In the future we may want to distinguish lite client layout from the others. */
    static void markLayoutLiteClient(const QnLayoutResourcePtr& layout);

    QThreadPool* threadPool() const;

signals:
    void resourceAdded(const QnResourcePtr& resource);
    void resourceRemoved(const QnResourcePtr& resource);
    void resourceChanged(const QnResourcePtr& resource);
    void statusChanged(const QnResourcePtr& resource, Qn::StatusChangeReason reason);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    mutable QnMutex m_resourcesMtx;
    bool m_tranInProgress;

    QnResourceList m_tmpResources;
    QHash<QnUuid, QnResourcePtr> m_resources;
    QHash<QnUuid, QnMediaServerResourcePtr> m_incompatibleServers;
    mutable QnUserResourcePtr m_adminResource;
    std::unique_ptr<QThreadPool> m_threadPool;
};
