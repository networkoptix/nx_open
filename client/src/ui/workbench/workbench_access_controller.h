#ifndef QN_WORKBENCH_ACCESS_CONTROLLER_H
#define QN_WORKBENCH_ACCESS_CONTROLLER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <client/client_globals.h>

#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QnWorkbenchContext;
class QnResourcePool;

class QnWorkbenchPermissionsNotifier: public QObject
{
    Q_OBJECT

public:
    QnWorkbenchPermissionsNotifier(QObject* parent = nullptr);

signals:
    void permissionsChanged(const QnResourcePtr& resource);

private:
    friend class QnWorkbenchAccessController;
};


/**
 * This class implements access control.
 */
class QnWorkbenchAccessController: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnWorkbenchAccessController(QObject* parent = nullptr);
    virtual ~QnWorkbenchAccessController();

    /**
     * \param resource                  Resource to get permissions for.
     * \returns                         Permissions for the given resource.
     */
    Qn::Permissions permissions(const QnResourcePtr& resource) const;

    /**
     * \param resource                  Resource to check permissions for.
     * \param requiredPermissions       Permissions to check.
     * \returns                         Whether actual permissions for the given resource
     *                                  include required permissions.
     */
    bool hasPermissions(const QnResourcePtr& resource, Qn::Permissions requiredPermissions) const;

    /**
     * \param resources                 List of resources to get combined permissions for.
     * \returns                         Bitwise AND combination of permissions for the provided resources.
     */
    Qn::Permissions combinedPermissions(const QnResourceList& resources) const;

    /**
     * \returns                         Global permissions of the current user,
     *                                  adjusted to take deprecation and superuser status into account.
     *                                  Same as <tt>permissions(context()->user())</tt>.
     */
    Qn::GlobalPermissions globalPermissions() const;

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
    QnWorkbenchPermissionsNotifier *notifier(const QnResourcePtr& resource) const;

    bool canCreateStorage(const QnUuid& serverId) const;
    bool canCreateLayout(const QnUuid& layoutParentId) const;
    bool canCreateUser(Qn::GlobalPermissions targetPermissions, bool isOwner) const;
    bool canCreateVideoWall() const;
    bool canCreateWebPage() const;

signals:
    /**
     * \param resource                  Resource for which permissions have changed.
     *                                  Guaranteed not to be null.
     */
    void permissionsChanged(const QnResourcePtr& resource);

protected slots:
    void updatePermissions(const QnResourcePtr& resource);

    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr& resource);

    void at_accessibleResourcesChanged(const QnUuid& userId);

private:
    void recalculateAllPermissions();

    void setPermissionsInternal(const QnResourcePtr& resource, Qn::Permissions permissions);

    Qn::Permissions calculatePermissions(const QnResourcePtr& resource) const;
    Qn::Permissions calculatePermissionsInternal(const QnLayoutResourcePtr& layout) const;
    Qn::GlobalPermissions calculateGlobalPermissions() const;

private:
    struct PermissionsData
    {
        PermissionsData(): permissions(0), notifier(NULL) {}

        Qn::Permissions permissions;
        QnWorkbenchPermissionsNotifier *notifier;
    };

    QnUserResourcePtr m_user;
    Qn::GlobalPermissions m_globalPermissions;
    bool m_readOnlyMode;
    mutable QHash<QnResourcePtr, PermissionsData> m_dataByResource;
};


#endif // QN_WORKBENCH_ACCESS_CONTROLLER_H
