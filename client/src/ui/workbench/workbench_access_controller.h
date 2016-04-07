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
    Qn::GlobalPermissions globalPermissions() const;

    /**
     * \param user                      User to get global permissions for.
     * \returns                         Global permissions of the given user,
     *                                  adjusted to take deprecation and superuser status into account.
     */
    Qn::GlobalPermissions globalPermissions(const QnUserResourcePtr &user) const;

    /**
     * \param requiredPermissions       Global permissions to check.
     * \returns                         Whether actual global permissions
     *                                  include required permissions.
     */
    bool hasGlobalPermission(Qn::GlobalPermission requiredPermissions) const;

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

    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
private:
    void recalculateAllPermissions();

    void setPermissionsInternal(const QnResourcePtr &resource, Qn::Permissions permissions);

    Qn::Permissions calculatePermissions(const QnResourcePtr &resource) const;

    Qn::Permissions calculatePermissionsInternal(const QnUserResourcePtr &user) const;
    Qn::Permissions calculatePermissionsInternal(const QnLayoutResourcePtr &layout) const;
    Qn::Permissions calculatePermissionsInternal(const QnVirtualCameraResourcePtr &camera) const;
    Qn::Permissions calculatePermissionsInternal(const QnAbstractArchiveResourcePtr &archive) const;
    Qn::Permissions calculatePermissionsInternal(const QnMediaServerResourcePtr &server) const;
    Qn::Permissions calculatePermissionsInternal(const QnVideoWallResourcePtr &videoWall) const;
    Qn::Permissions calculatePermissionsInternal(const QnWebPageResourcePtr &webPage) const;

private:
    struct PermissionsData {
        PermissionsData(): permissions(0), notifier(NULL) {}

        Qn::Permissions permissions;
        QnWorkbenchPermissionsNotifier *notifier;
    };

    QnUserResourcePtr m_user;
    Qn::GlobalPermissions m_userPermissions;
    bool m_readOnlyMode;
    mutable QHash<QnResourcePtr, PermissionsData> m_dataByResource;
};


#endif // QN_WORKBENCH_ACCESS_CONTROLLER_H
