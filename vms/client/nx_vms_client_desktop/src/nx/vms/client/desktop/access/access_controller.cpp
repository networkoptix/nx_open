// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "access_controller.h"

#include <memory>
#include <optional>

#include <client/client_runtime_settings.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/desktop/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

class AccessController::Private: public QObject
{
    AccessController* const q;

public:
    Private(AccessController* q);

    Qn::Permissions permissionsForArchive(const QnAbstractArchiveResourcePtr& archive) const;
    Qn::Permissions permissionsForCamera(const QnVirtualCameraResourcePtr& camera) const;
    Qn::Permissions permissionsForFileLayout(const QnFileLayoutResourcePtr& layout) const;
    Qn::Permissions permissionsForRemoteLayout(const LayoutResourcePtr& layout) const;
    Qn::Permissions permissionsForCloudLayout(const CrossSystemLayoutResourcePtr& layout) const;
    Qn::Permissions permissionsForUser(const QnUserResourcePtr& user) const;

private:
    void handleResourcesAdded(const QnResourceList& resources);
    void handleResourcesRemoved(const QnResourceList& resources);
};

// ------------------------------------------------------------------------------------------------
// AccessController

AccessController::AccessController(SystemContext* systemContext, QObject* parent):
    base_type(systemContext, parent),
    d(new Private{this})
{
}

AccessController::~AccessController()
{
    // Required here for forward declared scoped pointer destruction.
}

bool AccessController::canCreateStorage(const QnUuid& serverId) const
{
    return user() && systemContext()->resourceAccessManager()->canCreateStorage(user(), serverId);
}

bool AccessController::canCreateLayout(const nx::vms::api::LayoutData& layoutData) const
{
    return user()
        && systemContext()->resourceAccessManager()->canCreateLayout(user(), layoutData);
}

bool AccessController::canCreateUser(
    GlobalPermissions targetPermissions,
    const std::vector<QnUuid>& targetGroups) const
{
    return user() && systemContext()->resourceAccessManager()->canCreateUser(
        user(), targetPermissions, targetGroups);
}

bool AccessController::canCreateVideoWall() const
{
    return user() && systemContext()->resourceAccessManager()->canCreateVideoWall(user());
}

bool AccessController::canCreateWebPage() const
{
    return user() && systemContext()->resourceAccessManager()->canCreateWebPage(user());
}

Qn::Permissions AccessController::calculatePermissions(const QnResourcePtr& targetResource) const
{
    if (!NX_ASSERT(targetResource))
        return {};

    if (const auto archive = targetResource.objectCast<QnAbstractArchiveResource>())
        return d->permissionsForArchive(archive);

    if (const auto cloudLayout = targetResource.objectCast<CrossSystemLayoutResource>())
        return d->permissionsForCloudLayout(cloudLayout);

    // If a system the resource belongs to isn't connected yet, user must be able to view thumbnails
    // with the appropriate informations.
    if (const auto crossSystemCamera = targetResource.objectCast<CrossSystemCameraResource>())
    {
        const auto systemId = crossSystemCamera->systemId();
        const auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);

        if (context && !context->isSystemReadyToUse())
            return Qn::ViewContentPermission;
    }

    if (const auto fileLayout = targetResource.objectCast<QnFileLayoutResource>())
        return d->permissionsForFileLayout(fileLayout);

    if (const auto layout = targetResource.objectCast<LayoutResource>())
        return d->permissionsForRemoteLayout(layout);

    if (qnRuntime->isVideoWallMode())
        return Qn::VideoWallMediaPermissions;

    if (!user())
        return Qn::NoPermissions;

    if (const auto user = targetResource.objectCast<QnUserResource>())
        return d->permissionsForUser(user);

    if (const auto camera = targetResource.objectCast<QnVirtualCameraResource>())
        return d->permissionsForCamera(camera);

    return base_type::calculatePermissions(targetResource);
}

bool AccessController::isDeviceAccessRelevant(AccessRights requiredAccessRights) const
{
    if (qnRuntime->isVideoWallMode())
    {
        const auto kVideoWallModeAccessRights =
            AccessRight::view
            | AccessRight::viewArchive
            | AccessRight::viewBookmarks
            | AccessRight::userInput;

        return kVideoWallModeAccessRights.testFlags(requiredAccessRights);
    }

    if (qnRuntime->isAcsMode())
    {
        const auto kAcsModeAccessRights =
            AccessRight::view
            | AccessRight::viewArchive
            | AccessRight::exportArchive
            | AccessRight::viewBookmarks
            | AccessRight::userInput;

        return kAcsModeAccessRights.testFlags(requiredAccessRights);
    }

    return base_type::isDeviceAccessRelevant(requiredAccessRights);
}

GlobalPermissions AccessController::calculateGlobalPermissions() const
{
    return (qnRuntime->isVideoWallMode() || qnRuntime->isAcsMode() || !user())
        ? GlobalPermissions{}
        : base_type::calculateGlobalPermissions();
}

// ------------------------------------------------------------------------------------------------
// AccessController::Private

Qn::Permissions AccessController::Private::permissionsForArchive(
    const QnAbstractArchiveResourcePtr& /*archive*/) const
{
    return Qn::ReadPermission
        | Qn::ViewContentPermission
        | Qn::ViewLivePermission
        | Qn::ViewFootagePermission
        | Qn::ExportPermission
        | Qn::WritePermission
        | Qn::WriteNamePermission;
}

Qn::Permissions AccessController::Private::permissionsForCamera(
    const QnVirtualCameraResourcePtr& camera) const
{
    auto permissions = q->base_type::calculatePermissions(camera);

    if (camera->licenseType() == Qn::LC_VMAX && !camera->isScheduleEnabled())
        permissions &= ~(Qn::ViewLivePermission | Qn::ViewFootagePermission);

    if (camera->hasFlags(Qn::cross_system))
        permissions &= ~Qn::GenericEditPermissions;

    if (ini().allowToPutAnyAccessibleDeviceOnScene && permissions.testFlag(Qn::ReadPermission))
        permissions.setFlag(Qn::ViewContentPermission);

    return permissions;
}

Qn::Permissions AccessController::Private::permissionsForFileLayout(
    const QnFileLayoutResourcePtr& layout) const
{
    if (layout->isReadOnly() || layout->requiresPassword())
        return Qn::ReadPermission | Qn::RemovePermission | Qn::WriteNamePermission;

    return Qn::FullGenericPermissions
        | Qn::EditLayoutSettingsPermission
        | (layout->locked() ? Qn::NoPermissions : Qn::AddRemoveItemsPermission);
}

Qn::Permissions AccessController::Private::permissionsForRemoteLayout(
    const LayoutResourcePtr& layout) const
{
    // Some layouts are created with predefined permissions which are never changed.
    const QVariant presetPermissions = layout->data().value(Qn::LayoutPermissionsRole);
    if (presetPermissions.isValid() && presetPermissions.canConvert<int>())
    {
        NX_ASSERT(false, "Remote layouts with preset permissions should not have the context.");
        return static_cast<Qn::Permissions>(presetPermissions.toInt()) | Qn::ReadPermission;
    }

    const auto loggedIn = q->user()
        ? Qn::AllPermissions
        : ~(Qn::RemovePermission
            | Qn::SavePermission
            | Qn::WriteNamePermission
            | Qn::EditLayoutSettingsPermission);

    const auto locked = layout->locked()
        ? ~(Qn::AddRemoveItemsPermission | Qn::WriteNamePermission)
        : Qn::AllPermissions;

    // User can do everything with local layouts except removing from server.
    if (layout->hasFlags(Qn::local))
    {
        return locked & loggedIn
            & (static_cast<Qn::Permissions>(Qn::FullLayoutPermissions &~ Qn::RemovePermission));
    }

    if (qnRuntime->isVideoWallMode())
        return Qn::VideoWallLayoutPermissions;

    // No other resources must be available while we are logged out.
    // We may come here while receiving initial resources, when user is still not set.
    // In this case permissions will be recalculated on userChanged.
    if (!q->user())
        return Qn::NoPermissions;

    return q->base_type::calculatePermissions(layout);
}

Qn::Permissions AccessController::Private::permissionsForCloudLayout(
    const CrossSystemLayoutResourcePtr& layout) const
{
    // User can fully control his Cloud Layouts.
    return layout->locked()
        ? (Qn::FullLayoutPermissions & ~(Qn::AddRemoveItemsPermission | Qn::WriteNamePermission))
        : Qn::FullLayoutPermissions;
}

Qn::Permissions AccessController::Private::permissionsForUser(const QnUserResourcePtr& user) const
{
    // Check if we are creating new user.
    if (!user->resourcePool())
    {
        if (!q->hasGlobalPermissions(GlobalPermission::powerUser))
            return {};

        Qn::Permissions permissions = Qn::FullUserPermissions;
        if (user->isCloud())
            permissions = permissions & ~Qn::WriteDigestPermission;

        return permissions;
    }

    return q->base_type::calculatePermissions(user);
}

// ------------------------------------------------------------------------------------------------
// AccessController::Private

AccessController::Private::Private(AccessController* q): q(q)
{
    const auto resourcePool = q->systemContext()->resourcePool();
    handleResourcesAdded(resourcePool->getResources());

    connect(resourcePool, &QnResourcePool::resourcesAdded,
        this, &Private::handleResourcesAdded);

    connect(resourcePool, &QnResourcePool::resourcesRemoved,
        this, &Private::handleResourcesRemoved);
}

void AccessController::Private::handleResourcesAdded(const QnResourceList& resources)
{
    const auto updatePermissions =
        [this](const QnResourcePtr& resource) { q->updatePermissions({resource}); };

    for (const auto& resource: resources)
    {
        // Capture password setting or dropping for layout.
        if (const auto& fileLayout = resource.objectCast<QnFileLayoutResource>())
        {
            connect(fileLayout.get(), &QnFileLayoutResource::passwordChanged,
                this, updatePermissions);

            // `readOnly` may also change when re-reading encrypted layout.
            connect(fileLayout.get(), &QnFileLayoutResource::readOnlyChanged,
                this, updatePermissions);
        }
    }
}

void AccessController::Private::handleResourcesRemoved(const QnResourceList& resources)
{
    for (const auto& resource: resources)
        resource->disconnect(this);
}

} // namespace nx::vms::client::desktop
