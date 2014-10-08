#ifndef QN_WORKBENCH_ACCESS_CONTROLLER_H
#define QN_WORKBENCH_ACCESS_CONTROLLER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <client/client_globals.h>

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

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
class QnWorkbenchAccessController: public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT
    
    typedef Connective<QObject> base_type;
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
     * \returns                         Global permissions of the current user,
     *                                  adjusted to take deprecation and superuser status into account.
     *                                  Same as <tt>permissions(context()->user())</tt>.
     */
    Qn::Permissions globalPermissions() const;

    /**
     * \param user                      User to get global permissions for.
     * \returns                         Global permissions of the given user,
     *                                  adjusted to take deprecation and superuser status into account.
     */
    Qn::Permissions globalPermissions(const QnUserResourcePtr &user) const;

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

signals:
    /**
     * \param resource                  Resource for which permissions have changed. 
     *                                  Guaranteed not to be null.
     */
    void permissionsChanged(const QnResourcePtr &resource);

protected slots:
    void updatePermissions(const QnResourcePtr &resource);
    void updatePermissions(const QnLayoutResourcePtr &layout);
    void updatePermissions(const QnResourceList &resources);

    void at_context_userChanged(const QnUserResourcePtr &user);
    
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
private:
    void setPermissionsInternal(const QnResourcePtr &resource, Qn::Permissions permissions);

    Qn::Permissions calculatePermissions(const QnResourcePtr &resource) const;
    Qn::Permissions calculatePermissions(const QnUserResourcePtr &user) const;
    Qn::Permissions calculatePermissions(const QnLayoutResourcePtr &layout) const;
    Qn::Permissions calculatePermissions(const QnVirtualCameraResourcePtr &camera) const;
    Qn::Permissions calculatePermissions(const QnAbstractArchiveResourcePtr &archive) const;
    Qn::Permissions calculatePermissions(const QnMediaServerResourcePtr &server) const;
    Qn::Permissions calculatePermissions(const QnVideoWallResourcePtr &videoWall) const;

private:
    struct PermissionsData {
        PermissionsData(): permissions(0), notifier(NULL) {}

        Qn::Permissions permissions;
        QnWorkbenchPermissionsNotifier *notifier;
    };

    QnUserResourcePtr m_user;
    Qn::Permissions m_userPermissions;
    mutable QHash<QnResourcePtr, PermissionsData> m_dataByResource;
};


#endif // QN_WORKBENCH_ACCESS_CONTROLLER_H
