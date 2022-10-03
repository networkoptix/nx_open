// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>

#include <common/common_globals.h>
#include <core/resource/client_resource_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_access/resource_access_manager.h>
#include <nx/core/access/access_types.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/resource/resource_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::api { struct LayoutData; }

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
    public QObject,
    public nx::vms::client::desktop::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnWorkbenchAccessController(
        nx::vms::client::desktop::SystemContext* systemContext,
        nx::core::access::Mode resourceAccessMode,
        QObject* parent = nullptr);

    virtual ~QnWorkbenchAccessController() override;

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr& user);

    /** @returns Whether the current user has admin permissions. */
    bool hasAdminPermissions() const;

    /**
     * @param resource Resource to get permissions for.
     * @returns Permissions for the given resource.
     */
    Qn::Permissions permissions(const QnResourcePtr& resource) const;

    /**
     * @param resource Resource to check permissions for.
     * @param requiredPermissions Permissions to check.
     * @returns Whether actual permissions for the given resource include all required permissions.
     */
    bool hasPermissions(const QnResourcePtr& resource, Qn::Permissions requiredPermissions) const;

    /**
     * @param requiredPermissions Check whether the current user has the specified permision
     * to at least one resource in the resource pool.
     */
    bool anyResourceHasPermissions(Qn::Permissions requiredPermissions) const;

    /**
     * @returns Global permissions of the current user, adjusted to take deprecation and superuser
     * status into account. Same as <tt>permissions(context()->user())</tt>.
     */
    GlobalPermissions globalPermissions() const;

    /**
     * @param requiredPermissions Global permissions to check.
     * @returns Whether actual global permissions include required permissions.
     */
    bool hasGlobalPermission(GlobalPermission requiredPermission) const;
    bool hasGlobalPermissions(GlobalPermissions requiredPermissions) const;

    /**
     * @param resource Resource to get permissions change notifier for.
     * @returns Notifier that can be used to track permission changes for the given resource.
     */
    QnWorkbenchPermissionsNotifier* notifier(const QnResourcePtr& resource) const;

    bool canCreateStorage(const QnUuid& serverId) const;
    bool canCreateLayout(const nx::vms::api::LayoutData& layoutData) const;
    bool canCreateUser(GlobalPermissions targetPermissions, const std::vector<QnUuid>& targetGroups,
        bool isOwner) const;
    bool canCreateVideoWall() const;
    bool canCreateWebPage() const;

    /** Filter given list of resources to leave only accessible. */
    template <class Resource>
    QnSharedResourcePointerList<Resource> filtered(
        const QnSharedResourcePointerList<Resource>& source,
        Qn::Permissions requiredPermissions) const
    {
        QnSharedResourcePointerList<Resource> result;
        for (const QnSharedResourcePointer<Resource>& resource: source)
        {
            if (hasPermissions(resource, requiredPermissions))
                result.push_back(resource);
        }
        return result;
    }

signals:
    /** @param resource Resource for which permissions have changed. Guaranteed not to be null. */
    void permissionsChanged(const QnResourcePtr& resource);

    /** Notify that current user's permissions has been recalculated. */
    void permissionsReset();

    /** Notify that current user's global permissions were changed. */
    void globalPermissionsChanged();

    /** Notify that current user was changed. */
    void userChanged(QnUserResourcePtr& user);

protected slots:
    void updatePermissions(const QnResourcePtr& resource);

    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr& resource);

private:
    void recalculateGlobalPermissions();
    void recalculateAllPermissions();

    void setPermissionsInternal(const QnResourcePtr& resource, Qn::Permissions permissions);

    Qn::Permissions calculatePermissions(const QnResourcePtr& resource) const;
    // Deal with layouts; called from calculatePermissions().
    Qn::Permissions calculateRemoteLayoutPermissions(
        const nx::vms::client::desktop::LayoutResourcePtr& layout) const;
    Qn::Permissions calculateFileLayoutPermissions(const QnFileLayoutResourcePtr& layout) const;

    GlobalPermissions calculateGlobalPermissions() const;

private:
    const nx::core::access::Mode m_mode;
    struct PermissionsData
    {
        Qn::Permissions permissions;
        QnWorkbenchPermissionsNotifier* notifier = nullptr;
    };

    QnUserResourcePtr m_user;
    GlobalPermissions m_globalPermissions;
    QnResourceAccessManager::Notifier* const m_accessRightsNotifier;
    mutable QHash<QnResourcePtr, PermissionsData> m_dataByResource;
};
