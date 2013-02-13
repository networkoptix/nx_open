#ifndef resource_pool_h_1537
#define resource_pool_h_1537

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QObject>

#include "core/resource/resource.h"
#include "core/resource_managment/resource_criterion.h"
#include "core/resource/network_resource.h"

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
        //!do not check resources, omwned by another entites
        rfOnlyFriends,
        //!check all resources
        rfAllResources
    };

    QnResourcePool();
    ~QnResourcePool();

    static QnResourcePool *instance();

    // this function will add or update existing resources
    // keeps database ID ( if possible )
    void addResources(const QnResourceList &resources);

    inline void addResource(const QnResourcePtr &resource)
    { addResources(QnResourceList() << resource); }

    void removeResources(const QnResourceList &resources);
    inline void removeResource(const QnResourcePtr &resource)
    { removeResources(QnResourceList() << resource); }

    QnResourceList getResources() const;

    QnResourcePtr getResourceById(QnId id, Filter searchFilter = rfOnlyFriends) const;
    QnResourcePtr getResourceByGuid(QString guid) const;

    QnResourcePtr getResourceByUniqId(const QString &id) const;
    void updateUniqId(QnResourcePtr res, const QString &newUniqId);

    bool hasSuchResource(const QString &uniqid) const;

    QnResourcePtr getResourceByUrl(const QString &url) const;

    QnNetworkResourcePtr getNetResourceByPhysicalId(const QString &physicalId) const;
    QnNetworkResourcePtr getResourceByMacAddress(const QString &mac) const;

    QnNetworkResourceList getAllNetResourceByPhysicalId(const QString &mac) const;
    QnNetworkResourceList getAllNetResourceByHostAddress(const QString &hostAddress) const;
    QnNetworkResourceList getAllNetResourceByHostAddress(const QHostAddress &hostAddress) const;
    QnNetworkResourcePtr getEnabledResourceByPhysicalId(const QString &mac) const;
    QnResourceList getAllEnabledCameras() const;
    QnResourcePtr getEnabledResourceByUniqueId(const QString &uniqueId) const;

    // returns list of resources with such flag
    QnResourceList getResourcesWithFlag(QnResource::Flag flag) const;

    QnResourceList getResourcesWithParentId(QnId id) const;
    QnResourceList getResourcesWithTypeId(QnId id) const;

    QStringList allTags() const;

    int activeCameras() const;

    // TODO #gdm: this is a hack. Fix.
    bool isLayoutsUpdated() const;
    void setLayoutsUpdated(bool updateLayouts);

signals:
    void resourceAdded(const QnResourcePtr &resource);
    void resourceRemoved(const QnResourcePtr &resource);
    void resourceChanged(const QnResourcePtr &resource);
    void statusChanged(const QnResourcePtr &resource);

    void aboutToBeDestroyed();

private:
    mutable QMutex m_resourcesMtx;
    bool m_updateLayouts;
    QnResourcePtr localServer;
    QHash<QString, QnResourcePtr> m_resources;
    //!Resources with flag \a QnResource::foreign set
    /*!
        Using separate dictionary to minimize existing code modification
    */
    QHash<QString, QnResourcePtr> m_foreignResources;

    /*!
        \return true, if \a resource has been inserted. false - if updated existing resource
    */
    bool insertOrUpdateResource( const QnResourcePtr &resource, QHash<QString, QnResourcePtr>* const resourcePool );
};


#define qnResPool QnResourcePool::instance()

#endif //resource_pool_h_1537
