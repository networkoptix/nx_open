// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_access_controller.h"

#include <client/client_runtime_settings.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_pool.h>
#include <nx/streaming/abstract_archive_resource.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/cross_system/cross_system_camera_resource.h>
#include <nx/vms/client/desktop/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/system_context.h>

using namespace nx::vms::api;
using namespace nx::vms::client::desktop;
using namespace nx::core::access;

QnWorkbenchPermissionsNotifier::QnWorkbenchPermissionsNotifier(QObject* parent) :
    QObject(parent)
{
}

QnWorkbenchAccessController::QnWorkbenchAccessController(
    SystemContext* systemContext,
    Mode resourceAccessMode,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(systemContext),
    m_mode(resourceAccessMode),
	m_accessRightsNotifier(resourceAccessMode == Mode::cached
        ? resourceAccessManager()->createNotifier(this)
		: nullptr)
{
    if (m_mode == Mode::cached)
    {
        connect(resourcePool(), &QnResourcePool::resourceAdded, this,
            &QnWorkbenchAccessController::at_resourcePool_resourceAdded);

        connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
            &QnWorkbenchAccessController::at_resourcePool_resourceRemoved);

        connect(m_accessRightsNotifier, &QnResourceAccessManager::Notifier::resourceAccessChanged, this,
            [this](const QnUuid& subjectId)
            {
                if (NX_ASSERT(m_user && subjectId == m_user->getId()))
                    recalculateAllPermissions();
            });

        connect(resourceAccessManager(), &QnResourceAccessManager::resourceAccessReset,
            this, &QnWorkbenchAccessController::recalculateAllPermissions);

        connect(resourceAccessManager(), &QnResourceAccessManager::permissionsDependencyChanged,
            this, &QnWorkbenchAccessController::updatePermissions);

        const auto handleSubjectsChanged =
            [this](const QSet<QnUuid>& changedSubjectIds)
            {
                if (!m_user)
                    return;

                if (changedSubjectIds.contains(m_user->getId()))
                {
                    recalculateAllPermissions();
                    return;
                }

                const auto users = resourcePool()->getResourcesByIds<QnUserResource>(
                    changedSubjectIds);

                for (const auto& user: users)
                    updatePermissions(user);
            };

        connect(systemContext->globalPermissionsWatcher(),
            &GlobalPermissionsWatcher::ownGlobalPermissionsChanged, this,
            [this, handleSubjectsChanged](const QnUuid& subjectId)
            {
                QSet<QnUuid> affectedSubjectIds = {subjectId};
                affectedSubjectIds += this->systemContext()->accessSubjectHierarchy()->
                    recursiveMembers(affectedSubjectIds);

                handleSubjectsChanged(affectedSubjectIds);
            });

        connect(systemContext->accessSubjectHierarchy(),
            &nx::core::access::SubjectHierarchy::changed, this,
            [handleSubjectsChanged](
                const QSet<QnUuid>& /*added*/,
                const QSet<QnUuid>& /*removed*/,
                const QSet<QnUuid>& /*groupsWithChangedMembers*/,
                const QSet<QnUuid>& subjectsWithChangedParents)
            {
                handleSubjectsChanged(subjectsWithChangedParents);
            });

        recalculateGlobalPermissions();

        // Some (exported layout) resources do already exist at this point.
        // We should process them as newly added. Permission updates are included for free.
        for (const QnResourcePtr& resource: resourcePool()->getResources())
            at_resourcePool_resourceAdded(resource);
    }
}

QnWorkbenchAccessController::~QnWorkbenchAccessController()
{
}

QnUserResourcePtr QnWorkbenchAccessController::user() const
{
    return m_user;
}

void QnWorkbenchAccessController::setUser(const QnUserResourcePtr& user)
{
    if (m_user == user)
        return;

    m_user = user;

    if (m_mode == Mode::cached)
    {
        m_accessRightsNotifier->setSubjectId(m_user ? m_user->getId() : QnUuid{});
        recalculateAllPermissions();
    }

    emit userChanged(m_user);
}

Qn::Permissions QnWorkbenchAccessController::permissions(const QnResourcePtr& resource) const
{
    if (!NX_ASSERT(systemContext() == resource->systemContext()))
        return ResourceAccessManager::permissions(resource);

    if (m_mode == Mode::direct || !m_dataByResource.contains(resource))
        return calculatePermissions(resource);

    return m_dataByResource.value(resource).permissions;
}

Qn::Permissions QnWorkbenchAccessController::permissions(const QnUuid& targetId) const
{
    return resourceAccessManager()->permissions(m_user, targetId);
}

bool QnWorkbenchAccessController::hasPermissions(
    const QnResourcePtr& resource,
    Qn::Permissions requiredPermissions) const
{
    return (permissions(resource) & requiredPermissions) == requiredPermissions;
}

bool QnWorkbenchAccessController::hasPermissions(
    const QnUuid& targetId, Qn::Permissions requiredPermissions) const
{
    return (permissions(targetId) & requiredPermissions) == requiredPermissions;
}

bool QnWorkbenchAccessController::hasPermissionsForAnyDevice(
    Qn::Permissions requiredPermissions) const
{
    const auto devices = resourcePool()->getAllCameras(
        /*parentId*/QnUuid{}, /*ignoreDesktopCameras*/ true);

    return std::any_of(devices.begin(), devices.end(),
        [this, requiredPermissions](auto resource)
        {
            return (permissions(resource) & requiredPermissions) != 0;
        });
}

bool QnWorkbenchAccessController::hasPowerUserPermissions() const
{
    return hasGlobalPermission(GlobalPermission::powerUser);
}

GlobalPermissions QnWorkbenchAccessController::globalPermissions() const
{
    return m_mode == Mode::cached
        ? m_globalPermissions
        : calculateGlobalPermissions();
}

bool QnWorkbenchAccessController::hasGlobalPermission(GlobalPermission requiredPermission) const
{
    return globalPermissions().testFlag(requiredPermission);
}

bool QnWorkbenchAccessController::hasGlobalPermissions(GlobalPermissions requiredPermissions) const
{
    return (globalPermissions() & requiredPermissions) == requiredPermissions;
}

QnWorkbenchPermissionsNotifier* QnWorkbenchAccessController::notifier(
    const QnResourcePtr& resource) const
{
    if (!NX_ASSERT(m_dataByResource.contains(resource)))
        return nullptr;

    PermissionsData& data = m_dataByResource[resource];
    if (!data.notifier)
    {
        data.notifier = new QnWorkbenchPermissionsNotifier(
            const_cast<QnWorkbenchAccessController*>(this));
    }

    return data.notifier;
}

bool QnWorkbenchAccessController::canCreateStorage(const QnUuid& serverId) const
{
    return m_user && resourceAccessManager()->canCreateStorage(m_user, serverId);
}

bool QnWorkbenchAccessController::canCreateLayout(
    const nx::vms::api::LayoutData& layoutData) const
{
    return m_user && resourceAccessManager()->canCreateLayout(m_user, layoutData);
}

bool QnWorkbenchAccessController::canCreateUser(
    GlobalPermissions targetPermissions,
    const std::vector<QnUuid>& targetGroups) const
{
    return m_user && resourceAccessManager()->canCreateUser(
        m_user, targetPermissions, targetGroups);
}

bool QnWorkbenchAccessController::canCreateVideoWall() const
{
    return m_user && resourceAccessManager()->canCreateVideoWall(m_user);
}

bool QnWorkbenchAccessController::canCreateWebPage() const
{
    return m_user && resourceAccessManager()->canCreateWebPage(m_user);
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(
    const QnResourcePtr& resource) const
{
    NX_ASSERT(resource);

    if (const auto archive = resource.dynamicCast<QnAbstractArchiveResource>())
    {
        return Qn::ReadPermission
            | Qn::ViewContentPermission
            | Qn::ViewLivePermission
            | Qn::ViewFootagePermission
            | Qn::ExportPermission
            | Qn::WritePermission
            | Qn::WriteNamePermission;
    }

    // User can fully control his Cloud Layouts.
    if (const auto cloudLayout = resource.dynamicCast<CrossSystemLayoutResource>())
    {
        Qn::Permissions result = Qn::FullLayoutPermissions;
        if (cloudLayout->locked())
            result &= ~(Qn::AddRemoveItemsPermission | Qn::WriteNamePermission);
        return result;
    }

    // If a system the resource belongs is not connected yet, user must be able to view thumbs
    // with the appropriate informations.
    if (const auto crossSystemCameraResource = resource.dynamicCast<CrossSystemCameraResource>())
    {
        const auto systemId = crossSystemCameraResource->systemId();
        if (const auto context = appContext()->cloudCrossSystemManager()->systemContext(systemId);
            context && !context->isSystemReadyToUse())
        {
            return Qn::ViewContentPermission;
        }
    }

    if (const auto fileLayout = resource.dynamicCast<QnFileLayoutResource>())
        return calculateFileLayoutPermissions(fileLayout);

    // Note that QnFileLayoutResource case is treated above.
    if (const auto layout = resource.dynamicCast<LayoutResource>())
        return calculateRemoteLayoutPermissions(layout);

    if (qnRuntime->isVideoWallMode())
        return Qn::VideoWallMediaPermissions;

    // No other resources must be available while we are logged out.
    if (!m_user)
        return Qn::NoPermissions;

    if (const auto user = resource.dynamicCast<QnUserResource>())
    {
        // Check if we are creating new user.
        if (!user->resourcePool())
        {
            Qn::Permissions permissions = Qn::FullUserPermissions;
            if (user->isCloud())
                permissions = permissions & ~Qn::WriteDigestPermission;

            return hasGlobalPermission(GlobalPermission::powerUser) ? permissions : Qn::NoPermissions;
        }
    }

    const auto basePermissions = resourceAccessManager()->permissions(m_user, resource);

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        Qn::Permissions permissions = basePermissions;
        if (camera->licenseType() == Qn::LC_VMAX && !camera->isScheduleEnabled())
            permissions &= ~(Qn::ViewLivePermission | Qn::ViewFootagePermission);

        if (camera->hasFlags(Qn::cross_system))
            permissions &= ~(Qn::WritePermission | Qn::WriteNamePermission | Qn::SavePermission);

        // In desktop client, cameras are openable on the scene if they are generally accessible.
        if (permissions.testFlag(Qn::ReadPermission))
            permissions.setFlag(Qn::ViewContentPermission);

        return permissions;
    }

    return basePermissions;
}

Qn::Permissions QnWorkbenchAccessController::calculateRemoteLayoutPermissions(
    const LayoutResourcePtr& layout) const
{
    NX_ASSERT(layout);

    // TODO: #sivanov Code duplication with QnResourceAccessManager::calculatePermissionsInternal.

    // Some layouts are created with predefined permissions which are never changed.
    QVariant presetPermissions = layout->data().value(Qn::LayoutPermissionsRole);
    if (presetPermissions.isValid() && presetPermissions.canConvert<int>())
    {
        NX_ASSERT(false, "Remote layouts with preset permissions should not have the context.");
        return static_cast<Qn::Permissions>(presetPermissions.toInt()) | Qn::ReadPermission;
    }

    const auto loggedIn = m_user
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
    if (!m_user)
        return Qn::NoPermissions;

    return resourceAccessManager()->permissions(m_user, layout);
}

Qn::Permissions QnWorkbenchAccessController::calculateFileLayoutPermissions(
    const QnFileLayoutResourcePtr& layout) const
{
    NX_ASSERT(layout);

    if (layout->isReadOnly() || layout->requiresPassword())
        return Qn::ReadPermission | Qn::RemovePermission | Qn::WriteNamePermission;

    auto permissions = Qn::FullGenericPermissions
        | Qn::AddRemoveItemsPermission
        | Qn::EditLayoutSettingsPermission;

    if (layout->locked())
        permissions &= ~Qn::AddRemoveItemsPermission;

    return permissions;
}

GlobalPermissions QnWorkbenchAccessController::calculateGlobalPermissions() const
{
    return m_user
        ? systemContext()->resourceAccessManager()->globalPermissions(m_user)
        : GlobalPermissions{};
}

void QnWorkbenchAccessController::updatePermissions(const QnResourcePtr& resource)
{
    NX_ASSERT(m_mode == Mode::cached);
    setPermissionsInternal(resource, calculatePermissions(resource));
}

void QnWorkbenchAccessController::setPermissionsInternal(const QnResourcePtr& resource,
    Qn::Permissions permissions)
{
    if (m_dataByResource.contains(resource) &&
        permissions == this->permissions(resource))
    {
        return;
    }

    PermissionsData& data = m_dataByResource[resource];
    data.permissions = permissions;

    emit permissionsChanged(resource);
    if (data.notifier)
        emit data.notifier->permissionsChanged(resource);
}

void QnWorkbenchAccessController::recalculateGlobalPermissions()
{
    auto newGlobalPermissions = calculateGlobalPermissions();
    bool changed = newGlobalPermissions != m_globalPermissions;
    m_globalPermissions = newGlobalPermissions;

    if (changed)
        emit globalPermissionsChanged();
}

void QnWorkbenchAccessController::recalculateAllPermissions()
{
    recalculateGlobalPermissions();

    for (const QnResourcePtr& resource: resourcePool()->getResources())
        updatePermissions(resource);

    emit permissionsReset();
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr& resource)
{
    // Capture password setting or dropping for layout.
    if (const auto& fileLayout = resource.objectCast<QnFileLayoutResource>())
    {
        connect(fileLayout.get(), &QnFileLayoutResource::passwordChanged, this,
            &QnWorkbenchAccessController::updatePermissions);

        // `readOnly` may also change when re-reading encrypted layout.
        connect(fileLayout.get(), &QnFileLayoutResource::readOnlyChanged, this,
            &QnWorkbenchAccessController::updatePermissions);
    }

    updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceRemoved(const QnResourcePtr& resource)
{
    resource->disconnect(this);
    m_dataByResource.remove(resource);
}
