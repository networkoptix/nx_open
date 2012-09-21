#ifndef QN_WORKBENCH_ACCESS_CONTROLLER_H
#define QN_WORKBENCH_ACCESS_CONTROLLER_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include <core/resource/user_resource.h>
#include "workbench_context_aware.h"
#include "workbench_globals.h"

class QnWorkbenchContext;
class QnResourcePool;

class QnWorkbenchPermissionsNotifier: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchPermissionsNotifier(QObject *parent = NULL): QObject(parent) {}

signals:
    void permissionsChanged(const QnResourcePtr &resource);

private:
    friend class QnWorkbenchAccessController;
};


/**
 * This class implements access control.
 */
class QnWorkbenchAccessController: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchAccessController(QObject *parent = NULL);
    virtual ~QnWorkbenchAccessController();

    /**
     * \param resource                  Resource to get permissions for.
     * \returns                         Permissions for the given resource.
     *                                  Includes global permissions if requested for the current user.
     */
    Qn::Permissions permissions(const QnResourcePtr &resource) const;

    /**
     * \param resource                  Resource to check permissions for.
     * \param requiredPermissions       Permissions to check.
     * \returns                         Whether actual permissions for the given resource
     *                                  include required permissions.
     */
    bool hasPermissions(const QnResourcePtr &resource, Qn::Permissions requiredPermissions) const;
    
    /**
     * \param resources                 List of resources to get combined permissions for.
     * \returns                         Bitwise AND combination of permissions for the provided resources.
     */
    template<class ResourceList>
    Qn::Permissions permissions(const ResourceList &resources, const typename ResourceList::const_iterator * = NULL /* Let SFINAE filter out non-lists. */) const {
        Qn::Permissions result = Qn::AllPermissions;
        foreach(const QnResourcePtr &resource, resources)
            result &= permissions(resource);
        return result;
    }

    /**
     * \returns                         Global permissions of the current user. 
     *                                  Same as <tt>permissions(context()->user())</tt>.
     */
    Qn::Permissions globalPermissions() const;

    /**
     * \param requiredPermissions       Global permissions to check.
     * \returns                         Whether actual global permissions
     *                                  include required permissions.
     */
    bool hasGlobalPermissions(Qn::Permissions requiredPermissions) const;

    /**
     * \param resource                  Resource to get permissions change notifier for.
     * \returns                         Notifier that can be used to track permission changes for
     *                                  the given resource.
     */
    QnWorkbenchPermissionsNotifier *notifier(const QnResourcePtr &resource) const;

    // TODO: #Elric rename and add sane comment
    Qn::Permissions calculateGlobalPermissions(const QnUserResourcePtr &user);

signals:
    /**
     * \param resource                  Resource for which permissions have changed. 
     *                                  Guaranteed not to be null.
     */
    void permissionsChanged(const QnResourcePtr &resource);

protected slots:
    void updatePermissions(const QnResourcePtr &resource);
    void updatePermissions(const QnResourceList &resources);
    void updateSenderPermissions();

    void at_context_userChanged(const QnUserResourcePtr &user);
    
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

    void at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &layout);

private:
    void setPermissionsInternal(const QnResourcePtr &resource, Qn::Permissions permissions);

    Qn::Permissions calculatePermissions(const QnResourcePtr &resource);
    Qn::Permissions calculatePermissions(const QnUserResourcePtr &user);
    Qn::Permissions calculatePermissions(const QnLayoutResourcePtr &layout);
    Qn::Permissions calculatePermissions(const QnVirtualCameraResourcePtr &camera);
    Qn::Permissions calculatePermissions(const QnAbstractArchiveResourcePtr &archive);
    Qn::Permissions calculatePermissions(const QnVideoServerResourcePtr &server);

private:
    struct PermissionsData {
        PermissionsData(): permissions(0), notifier(NULL) {}

        Qn::Permissions permissions;
        mutable QnWorkbenchPermissionsNotifier *notifier;
    };

    QnUserResourcePtr m_user;
    Qn::Permissions m_userPermissions;
    QHash<QnResourcePtr, PermissionsData> m_dataByResource;
};


#endif // QN_WORKBENCH_ACCESS_CONTROLLER_H
