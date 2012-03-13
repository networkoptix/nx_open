#ifndef QN_WORKBENCH_ACCESS_CONTROLLER_H
#define QN_WORKBENCH_ACCESS_CONTROLLER_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include "workbench_context_aware.h"
#include "workbench_permissions.h"

class QnWorkbenchContext;
class QnResourcePool;

/**
 * This class implements access control.
 * 
 * It hides resources that cannot be viewed by the user that is currently logged in.
 */
class QnWorkbenchAccessController: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchAccessController(QObject *parent = NULL);
    virtual ~QnWorkbenchAccessController();

    Qn::Permissions permissions(const QnResourcePtr &resource) const {
        return m_permissionsByResource.value(resource);
    }

    template<class ResourceList>
    Qn::Permissions permissions(const ResourceList &resources, const typename ResourceList::const_iterator * = NULL /* Let SFINAE filter out non-lists. */) const {
        Qn::Permissions result = Qn::AllPermissions;
        foreach(const QnResourcePtr &resource, resources)
            result &= permissions(resource);
        return result;
    }

    Qn::Permissions permissions() const;

signals:
    void permissionsChanged(const QnResourcePtr &resource);

protected:
    bool isOwner() const;
    bool isAdmin() const;

protected slots:
    void updatePermissions(const QnResourcePtr &resource);
    void updatePermissions(const QnResourceList &resources);
    void updateSenderPermissions();

    void at_context_userChanged(const QnUserResourcePtr &user);
    
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    void setPermissionsInternal(const QnResourcePtr &resource, Qn::Permissions permissions);

    Qn::Permissions calculatePermissions(const QnResourcePtr &resource);
    Qn::Permissions calculatePermissions(const QnUserResourcePtr &user);
    Qn::Permissions calculatePermissions(const QnLayoutResourcePtr &layout);
    Qn::Permissions calculatePermissions(const QnVirtualCameraResourcePtr &camera);
    Qn::Permissions calculatePermissions(const QnVideoServerResourcePtr &server);
    Qn::Permissions calculateGlobalPermissions();

private:
    QnUserResourcePtr m_user;
    QHash<QnResourcePtr, Qn::Permissions> m_permissionsByResource;
};


#endif // QN_WORKBENCH_ACCESS_CONTROLLER_H
