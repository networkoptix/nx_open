#pragma once

#include <QtCore/QObject>
#include <QtCore/QList>

#include <client/client_globals.h>

#include <common/common_globals.h>
#include <common/common_module_aware.h>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
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
class NX_VMS_CLIENT_DESKTOP_API QnWorkbenchAccessController:
    public Connective<QObject>,
    public QnCommonModuleAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnWorkbenchAccessController(QnCommonModule* commonModule, QObject* parent = nullptr);
    virtual ~QnWorkbenchAccessController();

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr& user);

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
    GlobalPermissions globalPermissions() const;

    /**
     * \param requiredPermissions       Global permissions to check.
     * \returns                         Whether actual global permissions
     *                                  include required permissions.
     */
    bool hasGlobalPermission(GlobalPermission requiredPermissions) const;

    /**
     * \param resource                  Resource to get permissions change notifier for.
     * \returns                         Notifier that can be used to track permission changes for
     *                                  the given resource.
     */
    QnWorkbenchPermissionsNotifier *notifier(const QnResourcePtr& resource) const;

    bool canCreateStorage(const QnUuid& serverId) const;
    bool canCreateLayout(const QnUuid& layoutParentId) const;
    bool canCreateUser(GlobalPermissions targetPermissions, bool isOwner) const;
    bool canCreateVideoWall() const;
    bool canCreateWebPage() const;

    /* Filter given list of resources to leave only accessible. */
    template <class Resource>
    QnSharedResourcePointerList<Resource> filtered(const QnSharedResourcePointerList<Resource>& source,
        Qn::Permissions requiredPermissions) const
    {
        QnSharedResourcePointerList<Resource> result;
        for (const QnSharedResourcePointer<Resource>& resource : source)
            if (hasPermissions(resource, requiredPermissions))
                result.push_back(resource);
        return result;
    }


signals:
    /**
     * \param resource                  Resource for which permissions have changed.
     *                                  Guaranteed not to be null.
     */
    void permissionsChanged(const QnResourcePtr& resource);

    /** Notify that current user's global permissions were changed. */
    void globalPermissionsChanged();

protected slots:
    void updatePermissions(const QnResourcePtr& resource);

    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr& resource);

private:
    void recalculateAllPermissions();

    void setPermissionsInternal(const QnResourcePtr& resource, Qn::Permissions permissions);

    Qn::Permissions calculatePermissions(const QnResourcePtr& resource) const;
    // Deal with layouts; called from calculatePermissions().
    Qn::Permissions calculateLayoutPermissions(const QnLayoutResourcePtr& layout) const;

    GlobalPermissions calculateGlobalPermissions() const;

private:
    struct PermissionsData
    {
        PermissionsData(): permissions(0), notifier(NULL) {}

        Qn::Permissions permissions;
        QnWorkbenchPermissionsNotifier *notifier;
    };

    QnUserResourcePtr m_user;
    GlobalPermissions m_globalPermissions;
    mutable QHash<QnResourcePtr, PermissionsData> m_dataByResource;
};
