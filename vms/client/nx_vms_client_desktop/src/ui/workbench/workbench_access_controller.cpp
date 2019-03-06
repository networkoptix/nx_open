#include "workbench_access_controller.h"

#include <cassert>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/global_permissions_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/file_layout_resource.h>

#include <nx/streaming/abstract_archive_resource.h>

#include <utils/common/checked_cast.h>
#include <nx/vms/client/desktop/resources/layout_password_management.h>
#include <core/resource_management/resource_runtime_data.h>

QnWorkbenchPermissionsNotifier::QnWorkbenchPermissionsNotifier(QObject* parent) :
    QObject(parent)
{
}

QnWorkbenchAccessController::QnWorkbenchAccessController(
    QnCommonModule* commonModule,
    QObject* parent)
    :
    base_type(parent),
    QnCommonModuleAware(commonModule),
    m_user()
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnWorkbenchAccessController::at_resourcePool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnWorkbenchAccessController::at_resourcePool_resourceRemoved);

    connect(resourceAccessManager(), &QnResourceAccessManager::permissionsChanged, this,
        [this](const QnResourceAccessSubject& subject, const QnResourcePtr& resource,
            Qn::Permissions /*permissions*/)
        {
            if (!m_user || subject.user() != m_user)
                return;

            updatePermissions(resource);
        });

    connect(globalPermissionsManager(), &QnGlobalPermissionsManager::globalPermissionsChanged,
        this,
        [this](const QnResourceAccessSubject& subject, GlobalPermissions /*value*/)
        {
            if (!subject.user())
                return;

            if (subject.user() == m_user)
                recalculateAllPermissions();
            else
                updatePermissions(subject.user());
        });

    recalculateAllPermissions();
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
    recalculateAllPermissions();
}

Qn::Permissions QnWorkbenchAccessController::permissions(const QnResourcePtr& resource) const
{
    if (!m_dataByResource.contains(resource))
        return calculatePermissions(resource);

    return m_dataByResource.value(resource).permissions;
}

Qn::Permissions QnWorkbenchAccessController::combinedPermissions(const QnResourceList& resources) const
{
    Qn::Permissions result = Qn::AllPermissions;
    for (const QnResourcePtr& resource : resources)
        result &= permissions(resource);
    return result;
}

bool QnWorkbenchAccessController::hasPermissions(const QnResourcePtr& resource, Qn::Permissions requiredPermissions) const
{
    return (permissions(resource) & requiredPermissions) == requiredPermissions;
}

GlobalPermissions QnWorkbenchAccessController::globalPermissions() const
{
    return m_globalPermissions;
}

bool QnWorkbenchAccessController::hasGlobalPermission(GlobalPermission requiredPermission) const
{
    return m_globalPermissions.testFlag(requiredPermission);
}

QnWorkbenchPermissionsNotifier *QnWorkbenchAccessController::notifier(const QnResourcePtr& resource) const
{
    NX_ASSERT(m_dataByResource.contains(resource));

    if (!m_dataByResource.contains(resource))
        return NULL;

    PermissionsData& data = m_dataByResource[resource];
    if (!data.notifier)
        data.notifier = new QnWorkbenchPermissionsNotifier(const_cast<QnWorkbenchAccessController *>(this));
    return data.notifier;
}

bool QnWorkbenchAccessController::canCreateStorage(const QnUuid& serverId) const
{
    if (!m_user)
        return false;
    return resourceAccessManager()->canCreateStorage(m_user, serverId);
}

bool QnWorkbenchAccessController::canCreateLayout(const QnUuid& layoutParentId) const
{
    if (!m_user)
        return false;
    return resourceAccessManager()->canCreateLayout(m_user, layoutParentId);
}

bool QnWorkbenchAccessController::canCreateUser(GlobalPermissions targetPermissions, bool isOwner) const
{
    if (!m_user)
        return false;
    return resourceAccessManager()->canCreateUser(m_user, targetPermissions, isOwner);
}

bool QnWorkbenchAccessController::canCreateVideoWall() const
{
    if (!m_user)
        return false;
    return resourceAccessManager()->canCreateVideoWall(m_user);
}

bool QnWorkbenchAccessController::canCreateWebPage() const
{
    if (!m_user)
        return false;
    return resourceAccessManager()->canCreateWebPage(m_user);
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(
    const QnResourcePtr& resource) const
{
    NX_ASSERT(resource);

    if (QnAbstractArchiveResourcePtr archive = resource.dynamicCast<QnAbstractArchiveResource>())
    {
        return Qn::ReadPermission
            | Qn::ViewContentPermission
            | Qn::ViewLivePermission
            | Qn::ViewFootagePermission
            | Qn::ExportPermission
            | Qn::WritePermission
            | Qn::WriteNamePermission;
    }

    if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
        return calculateLayoutPermissions(layout);

    if (qnRuntime->isVideoWallMode())
        return Qn::VideoWallMediaPermissions;

    /* No other resources must be available while we are logged out. */
    if (!m_user)
        return Qn::NoPermissions;

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        /* Check if we are creating new user */
        if (!user->resourcePool())
        {
            return hasGlobalPermission(GlobalPermission::admin)
                ? Qn::FullUserPermissions
                : Qn::NoPermissions;
        }
    }

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (camera->licenseType() == Qn::LC_VMAX && !camera->isLicenseUsed())
        {
            const Qn::Permissions forbidden = Qn::ViewLivePermission | Qn::ViewFootagePermission;
            const auto basePermissions = resourceAccessManager()->permissions(m_user, resource);
            return basePermissions & ~forbidden;
        }
    }

    return resourceAccessManager()->permissions(m_user, resource);
}

Qn::Permissions QnWorkbenchAccessController::calculateLayoutPermissions(
    const QnLayoutResourcePtr& layout) const
{
    using namespace nx::vms::client::desktop;
    NX_ASSERT(layout);

    // TODO: #GDM Code duplication with QnResourceAccessManager::calculatePermissionsInternal
    const auto readOnly = commonModule()->isReadOnly()
        ? ~(Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission)
        : Qn::AllPermissions;

    // Some layouts are created with predefined permissions which are never changed.
    QVariant presetPermissions = layout->data().value(Qn::LayoutPermissionsRole);
    if (presetPermissions.isValid() && presetPermissions.canConvert<int>())
        return (static_cast<Qn::Permissions>(presetPermissions.toInt()) & readOnly) | Qn::ReadPermission;

    // Deal with normal (non explicitly-authorized) files separately.
    if (auto fileLayout = layout.dynamicCast<QnFileLayoutResource>())
    {
        if(fileLayout->requiresPassword())
            return Qn::WriteNamePermission | Qn::ReadPermission;

        auto permissions = Qn::ReadWriteSavePermission
            | Qn::AddRemoveItemsPermission
            | Qn::EditLayoutSettingsPermission
            | Qn::WriteNamePermission;

        if (layout->locked())
            permissions &= ~Qn::AddRemoveItemsPermission;

        return permissions;
    }

    const auto loggedIn = !m_user
        ? ~(Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission)
        : Qn::AllPermissions;

    const auto locked = layout->locked()
        ? ~(Qn::AddRemoveItemsPermission | Qn::WriteNamePermission)
        : Qn::AllPermissions;

    /* User can do everything with local layouts except removing from server. */
    if (layout->hasFlags(Qn::local))
    {
        return locked & loggedIn & readOnly
            & (static_cast<Qn::Permissions>(Qn::FullLayoutPermissions &~ Qn::RemovePermission));
    }

    if (qnRuntime->isVideoWallMode())
        return Qn::VideoWallLayoutPermissions;

    /*
     * No other resources must be available while we are logged out.
     * We may come here while receiving initial resources, when user is still not set.
     * In this case permissions will be recalculated on userChanged.
     */
    if (!m_user)
        return Qn::NoPermissions;

    return resourceAccessManager()->permissions(m_user, layout);
}

GlobalPermissions QnWorkbenchAccessController::calculateGlobalPermissions() const
{
    if (qnRuntime->isVideoWallMode())
        return GlobalPermission::videowallModePermissions;

    if (qnRuntime->isAcsMode())
        return GlobalPermission::acsModePermissions;

    if (!m_user)
        return {};

    return resourceAccessManager()->globalPermissions(m_user);
}

void QnWorkbenchAccessController::updatePermissions(const QnResourcePtr& resource)
{
    setPermissionsInternal(resource, calculatePermissions(resource));
}

void QnWorkbenchAccessController::setPermissionsInternal(const QnResourcePtr& resource,
    Qn::Permissions permissions)
{
    if (m_dataByResource.contains(resource) &&
        permissions == this->permissions(resource))
        return;

    PermissionsData& data = m_dataByResource[resource];
    data.permissions = permissions;

    emit permissionsChanged(resource);
    if (data.notifier)
        emit data.notifier->permissionsChanged(resource);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWorkbenchAccessController::recalculateAllPermissions()
{
    auto newGlobalPermissions = calculateGlobalPermissions();
    bool changed = newGlobalPermissions != m_globalPermissions;
    m_globalPermissions = newGlobalPermissions;

    if (changed)
        emit globalPermissionsChanged();

    for (const QnResourcePtr& resource: resourcePool()->getResources())
        updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr& resource)
{
    connect(resource, &QnResource::flagsChanged, this,
        &QnWorkbenchAccessController::updatePermissions);

    connect(resource, &QnResource::flagsChanged, this,
        &QnWorkbenchAccessController::updatePermissions);

    if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        connect(camera, &QnVirtualCameraResource::licenseUsedChanged, this,
            &QnWorkbenchAccessController::updatePermissions);
    }

    // Capture password setting or dropping for layout.
    if (const auto& fileLayout = resource.dynamicCast<QnFileLayoutResource>())
    {
        connect(fileLayout, &QnFileLayoutResource::passwordChanged, this,
            &QnWorkbenchAccessController::updatePermissions);
    }

    updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceRemoved(const QnResourcePtr& resource)
{
    resource->disconnect(this);
    m_dataByResource.remove(resource);
}

