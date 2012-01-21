#ifndef resource_pool_h_1537
#define resource_pool_h_1537

#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QObject>

#include "core/resource/resource.h"
#include "core/resourcemanagment/resource_criteria.h"
#include "core/resource/network_resource.h"

class QnResource;
class QnNetworkResource;
class CLRecorderDevice;

// this class holds all resources in the system which are READY TO BE USED( as long as resource is in the pool => share pointer counter >= 1)
// so if resource is in the pool it guaranties it cannot be deleted
// in addition to that it also can give list of resources based on some criteria
// and helps to administrate resources


// if resource is conflicting it must not be placed in resource

#define qnResPool QnResourcePool::instance()

class QN_EXPORT QnResourcePool : public QObject
{
    Q_OBJECT

public:
    QnResourcePool();
    ~QnResourcePool();

    static QnResourcePool *instance();

    void addResources(const QnResourceList &resources);
    inline void addResource(const QnResourcePtr &resource)
    { addResources(QnResourceList() << resource); }

    void removeResources(const QnResourceList &resources);
    inline void removeResource(const QnResourcePtr &resource)
    { removeResources(QnResourceList() << resource); }

    QnResourceList getResources() const;

    QnResourcePtr getResourceById(const QnId &id) const;

    QnResourcePtr getResourceByUniqId(const QString &id) const;

    bool hasSuchResouce(const QString &uniqid) const;


    QnResourcePtr getResourceByUrl(const QString &url) const;

    QnNetworkResourcePtr getNetResourceByMac(const QString &mac) const;

    // returns list of resources with such flag
    QnResourceList getResourcesWithFlag(unsigned long flag) const;

    QnResourceList getResourcesWithParentId(const QnId &id) const;


    QStringList allTags() const;

    QnResourceList findResourcesByCriteria(const CLDeviceCriteria &cr) const;

Q_SIGNALS:
    void resourceAdded(const QnResourcePtr &resource);
    void resourceRemoved(const QnResourcePtr &resource);
    void resourceChanged(const QnResourcePtr &resource);

private Q_SLOTS:
    void handleResourceChange();

private:
    bool isResourceMeetCriteria(const CLDeviceCriteria &cr, QnResourcePtr res) const;
    bool match_subfilter(QnResourcePtr resource, const QString &fltr) const;

private:
    mutable QMutex m_resourcesMtx;
    QnResourcePtr localServer;
    QHash<QnId, QnResourcePtr> m_resources;
    QHash<QnId, QnResourceList> m_resourceTree;
};

#endif //resource_pool_h_1537
