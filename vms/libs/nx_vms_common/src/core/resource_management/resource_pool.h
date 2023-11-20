// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <vector>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <api/helpers/camera_id_helper.h>
#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/string.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

class QThreadPool;
namespace nx::vms::common { class SystemContext; }

/**
 * This class holds all resources in the system that are READY TO BE USED (as long as resource is
 * in the pool => shared pointer counter >= 1).
 *
 * If resource is in the pool it is guaranteed that it won't be deleted.
 *
 * Resource pool can also give a list of resources based on some criteria and helps to administrate
 * resources.
 */
class NX_VMS_COMMON_API QnResourcePool:
    public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum Filter
    {
        /** Do not check resources owned by another entities. */
        OnlyFriends,
        /** Check all resources. */
        AllResources
    };

    explicit QnResourcePool(nx::vms::common::SystemContext* context, QObject* parent = nullptr);
    ~QnResourcePool();

    /**
     * Context of the System this instance belongs to.
     */
    nx::vms::common::SystemContext* systemContext() const;

    enum AddResourceFlag
    {
        NoAddResourceFlags = 0x00,
        SkipAddingTransaction = 0x01, //< Add resource instantly even if transaction is open.
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

    void beginTran();
    void commit();

    void removeResources(const QnResourceList& resources);

    void removeResource(const QnResourcePtr& resource)
    {
        NX_DEBUG(this, "About to remove resource");
        removeResources(QnResourceList() << resource);
    }

    //!Empties all internal dictionaries. Needed for correct destruction order at application stop
    void clear();

    using ResourceFilter = nx::vms::common::ResourceFilter;

    template<class Resource>
    using ResourceClassFilter = nx::vms::common::ResourceClassFilter<Resource>;

    //---------------------------------------------------------------------------------------------
    // Methods to get all resources.

    QnResourceList getResources() const;

    template<class Resource>
    QnSharedResourcePointerList<Resource> getResources() const
    {
        NX_READ_LOCKER locker(&m_resourcesMutex);
        return getResourcesUnsafe<Resource>();
    }

    //---------------------------------------------------------------------------------------------
    // Methods to get resources by id.

    template<class IdList>
    QnResourceList getResourcesByIds(IdList idList) const
    {
        NX_READ_LOCKER locker(&m_resourcesMutex);
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
    QnSharedResourcePointerList<Resource> getResourcesByIds(
        const IdList& idList,
        std::function<QnUuid(const typename IdList::value_type&)> id =
            [](const auto& item) { return item; }
    ) const
    {
        NX_READ_LOCKER locker(&m_resourcesMutex);
        QnSharedResourcePointerList<Resource> result;
        for (const auto& item: idList)
        {
            const auto itr = m_resources.find(id(item));
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
        NX_READ_LOCKER locker(&m_resourcesMutex);
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
        NX_READ_LOCKER locker(&m_resourcesMutex);
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

    template<class Resource>
    bool contains(ResourceClassFilter<Resource> filter) const
    {
        NX_READ_LOCKER locker(&m_resourcesMutex);
        QnSharedResourcePointerList<Resource> result;
        for (const QnResourcePtr& resource: m_resources)
        {
            if (auto derived = resource.template dynamicCast<Resource>())
            {
                if (filter(derived))
                    return true;
            }
        }
        return false;
    }

    QnNetworkResourceList getAllNetResourceByHostAddress(const nx::String& hostAddress) const;

    QnVirtualCameraResourceList getAllCameras(
        const QnUuid& parentId = {}, bool ignoreDesktopCameras = false) const;
    QnVirtualCameraResourceList getAllCameras(
        const QnResourcePtr& server,  bool ignoreDesktopCameras = false) const;

    /**
     * All Servers in the System.
     */
    QnMediaServerResourceList servers() const;

    /** All Storages in the System. */
    QnStorageResourceList storages() const;

    /**
     * All Servers with given status in the System.
     */
    QnMediaServerResourceList getAllServers(nx::vms::api::ResourceStatus status) const;

    /** Find an online server with actual internet access. */
    QnMediaServerResourcePtr serverWithInternetAccess() const;

    /**
     * Find online Server that has connection to Cloud and chosen as the one to sync data with it.
     */
    QnMediaServerResourcePtr masterCloudSyncServer() const;

    QnResourceList getResourcesByParentId(const QnUuid& parentId) const;

    // Returns list of resources with such flag.
    QnResourceList getResourcesWithFlag(Qn::ResourceFlag flag) const;

    //---------------------------------------------------------------------------------------------
    // Methods to get single resource by various conditions.

    QnResourcePtr getResource(ResourceFilter filter) const
    {
        NX_READ_LOCKER locker(&m_resourcesMutex);
        auto itr = std::find_if(m_resources.begin(), m_resources.end(), filter);
        return itr != m_resources.end() ? itr.value() : QnResourcePtr();
    }

    template<class Resource>
    QnSharedResourcePointer<Resource> getResource(ResourceClassFilter<Resource> filter) const
    {
        NX_READ_LOCKER locker(&m_resourcesMutex);
        for (const QnResourcePtr& resource: m_resources)
        {
            if (auto derived = resource.template dynamicCast<Resource>())
                if (filter(derived))
                    return derived;
        }
        return QnSharedResourcePointer<Resource>();
    }

    template<class Resource>
    QnSharedResourcePointer<Resource> getResourceByPhysicalId(const QString& id) const
    {
        return getNetworkResourceByPhysicalId(id).dynamicCast<Resource>();
    }

    template<class Resource>
    QnSharedResourcePointer<Resource> getResourceById(const QnUuid& id) const
    {
        NX_READ_LOCKER locker(&m_resourcesMutex);
        auto itr = m_resources.find(id);
        return itr != m_resources.end()
            ? itr.value().template dynamicCast<Resource>()
            : QnSharedResourcePointer<Resource>();
    }

    QnResourcePtr getResourceById(const QnUuid& id) const;

    // Shared id is groupId for multichannel resources and physicalId for single channel resources.
    QnSecurityCamResourceList getResourcesBySharedId(const QString& sharedId) const;

    /**
     * Returns resource list by logicalId. logicaId is a some camera tag used for integration
     * with external systems.
     */
    QnResourceList getResourcesByLogicalId(int value) const;

    QnResourcePtr getResourceByDescriptor(
        const nx::vms::common::ResourceDescriptor& descriptor) const;

    QnResourcePtr getResourceByUrl(
        const QString& url, const QnUuid& parentServerId = QnUuid()) const;

    QnNetworkResourcePtr getNetworkResourceByPhysicalId(const QString& physicalId) const;
    QnNetworkResourcePtr getResourceByMacAddress(const QString& mac) const;

    QnUserResourcePtr getAdministrator() const;

    std::pair<QnUserResourcePtr, bool /*hasClash*/> userByName(const QString& name) const;

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

    QThreadPool* threadPool() const;

signals:
    /**
     * Emitted whenever any new resource is added to the pool. In case of batch additions is called
     * for each resource separately.
     */
    [[deprecated]]
    void resourceAdded(const QnResourcePtr& resource);

    /**
     * Emitted whenever any new resource is added to the pool.
     */
    void resourcesAdded(const QnResourceList& resources);

    /**
     * Emitted whenever any resource is removed from the pool. In case of batch removings is called
     * for each resource separately.
     */
    [[deprecated]]
    void resourceRemoved(const QnResourcePtr& resource);

    /**
     * Emitted whenever any resource is removed from the pool.
     */
    void resourcesRemoved(const QnResourceList& resources);

    void resourceChanged(const QnResourcePtr& resource);
    void statusChanged(const QnResourcePtr& resource, Qn::StatusChangeReason reason);

private:
    template<class Resource>
    QnSharedResourcePointerList<Resource> getResourcesUnsafe() const
    {
        QnSharedResourcePointerList<Resource> result;
        for (const QnResourcePtr& resource: m_resources)
        {
            if (auto derived = resource.template dynamicCast<Resource>())
                result.push_back(derived);
        }
        return result;
    }

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    mutable nx::ReadWriteLock m_resourcesMutex;
    bool m_tranInProgress;

    QnResourceList m_tmpResources;
    QHash<QnUuid, QnResourcePtr> m_resources;
    mutable QnUserResourcePtr m_adminResource;
    std::unique_ptr<QThreadPool> m_threadPool;
};
