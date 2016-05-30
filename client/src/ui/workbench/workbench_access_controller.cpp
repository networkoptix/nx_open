#include "workbench_access_controller.h"

#include <cassert>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <common/common_module.h>

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>

#include <nx/streaming/abstract_archive_resource.h>

#include <utils/common/checked_cast.h>

#include "workbench_context.h"
#include "workbench_layout_snapshot_manager.h"



QnWorkbenchAccessController::QnWorkbenchAccessController(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
    , m_user()
    , m_globalPermissions(Qn::NoGlobalPermissions)
    , m_readOnlyMode(false)

{
    connect(qnResPool,          &QnResourcePool::resourceAdded,                     this,   &QnWorkbenchAccessController::at_resourcePool_resourceAdded);
    connect(qnResPool,          &QnResourcePool::resourceRemoved,                   this,   &QnWorkbenchAccessController::at_resourcePool_resourceRemoved);
    connect(snapshotManager(),  &QnWorkbenchLayoutSnapshotManager::flagsChanged,    this,   &QnWorkbenchAccessController::updatePermissions);

    connect(context(),          &QnWorkbenchContext::userChanged,                   this,   &QnWorkbenchAccessController::recalculateAllPermissions);
    connect(qnCommon,           &QnCommonModule::readOnlyChanged,                   this,   &QnWorkbenchAccessController::recalculateAllPermissions);
    connect(qnResourceAccessManager, &QnResourceAccessManager::accessibleResourcesChanged, this, &QnWorkbenchAccessController::at_accessibleResourcesChanged);

    recalculateAllPermissions();
}

QnWorkbenchAccessController::~QnWorkbenchAccessController() {
}

Qn::Permissions QnWorkbenchAccessController::permissions(const QnResourcePtr &resource) const
{
    if (!m_dataByResource.contains(resource))
        return calculatePermissions(resource);

    return m_dataByResource.value(resource).permissions;
}

Qn::Permissions QnWorkbenchAccessController::combinedPermissions(const QnResourceList &resources) const
{
    Qn::Permissions result = Qn::AllPermissions;
    for (const QnResourcePtr &resource : resources)
        result &= permissions(resource);
    return result;
}

bool QnWorkbenchAccessController::hasPermissions(const QnResourcePtr &resource, Qn::Permissions requiredPermissions) const
{
    return (permissions(resource) & requiredPermissions) == requiredPermissions;
}

Qn::GlobalPermissions QnWorkbenchAccessController::globalPermissions() const
{
    return m_globalPermissions;
}

bool QnWorkbenchAccessController::hasGlobalPermission(Qn::GlobalPermission requiredPermission) const
{
    return m_globalPermissions.testFlag(requiredPermission);
}

QnWorkbenchPermissionsNotifier *QnWorkbenchAccessController::notifier(const QnResourcePtr &resource) const
{
    NX_ASSERT(m_dataByResource.contains(resource));

    if(!m_dataByResource.contains(resource))
        return NULL;

    PermissionsData &data = m_dataByResource[resource];
    if(!data.notifier)
        data.notifier = new QnWorkbenchPermissionsNotifier(const_cast<QnWorkbenchAccessController *>(this));
    return data.notifier;
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissions(const QnResourcePtr &resource) const
{
    NX_ASSERT(resource);

    if (QnAbstractArchiveResourcePtr archive = resource.dynamicCast<QnAbstractArchiveResource>())
        return Qn::ReadPermission | Qn::ExportPermission;

    if (QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>())
        return calculatePermissionsInternal(layout);

    /* No other resources must be available while we are logged out. */
    if (!m_user)
        return Qn::NoPermissions;

    if (QnUserResourcePtr user = resource.dynamicCast<QnUserResource>())
    {
        /* Check if we are creating new user */
        if (user->flags().testFlag(Qn::local))
            return hasGlobalPermission(Qn::GlobalAdminPermission) ? Qn::FullUserPermissions : Qn::NoPermissions;
    }

    return qnResourceAccessManager->permissions(m_user, resource);
}

Qn::Permissions QnWorkbenchAccessController::calculatePermissionsInternal(const QnLayoutResourcePtr &layout) const
{
    NX_ASSERT(layout);

    //TODO: #GDM Code duplication with QnResourceAccessManager::calculatePermissionsInternal
    auto checkReadOnly = [this](Qn::Permissions permissions)
    {
        if (!m_readOnlyMode)
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLoggedIn = [this](Qn::Permissions permissions)
    {
        if (m_user)
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::SavePermission | Qn::WriteNamePermission | Qn::EditLayoutSettingsPermission);
    };

    auto checkLocked = [this, layout](Qn::Permissions permissions)
    {
        if (!layout->locked())
            return permissions;
        return permissions &~ (Qn::RemovePermission | Qn::AddRemoveItemsPermission | Qn::WriteNamePermission);
    };

    if (layout->isFile())
        return checkLocked(Qn::ReadWriteSavePermission | Qn::AddRemoveItemsPermission | Qn::EditLayoutSettingsPermission);

    /* User can do everything with local layouts except removing from server. */
    if (snapshotManager()->isLocal(layout))
        return checkLocked(checkLoggedIn(checkReadOnly(static_cast<Qn::Permissions>(Qn::FullLayoutPermissions &~Qn::RemovePermission))));

    /*
     * No other resources must be available while we are logged out.
     * We may come here while receiving initial resources, when user is still not set.
     * In this case permissions will be recalculated on userChanged.
     */
    if (!m_user)
        return Qn::NoPermissions;

    QVariant permissions = layout->data().value(Qn::LayoutPermissionsRole);
    if (permissions.isValid() && permissions.canConvert<int>())
        return checkReadOnly(static_cast<Qn::Permissions>(permissions.toInt())); // TODO: #Elric listen to changes

    return qnResourceAccessManager->permissions(m_user, layout);
}

Qn::GlobalPermissions QnWorkbenchAccessController::calculateGlobalPermissions() const
{
    if (qnRuntime->isVideoWallMode())
        return Qn::GlobalVideoWallModePermissionSet;

    if (qnRuntime->isActiveXMode())
        return Qn::GlobalActiveXModePermissionSet;

    if (!m_user)
        return Qn::NoGlobalPermissions;

    return qnResourceAccessManager->globalPermissions(m_user);
}

void QnWorkbenchAccessController::updatePermissions(const QnResourcePtr &resource) {
    setPermissionsInternal(resource, calculatePermissions(resource));
}

void QnWorkbenchAccessController::setPermissionsInternal(const QnResourcePtr &resource, Qn::Permissions permissions) {
    if(m_dataByResource.contains(resource) &&
        permissions == this->permissions(resource))
        return;

    PermissionsData &data = m_dataByResource[resource];
    data.permissions = permissions;

    emit permissionsChanged(resource);
    if(data.notifier)
        emit data.notifier->permissionsChanged(resource);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //

void QnWorkbenchAccessController::recalculateAllPermissions()
{
    m_user = context()->user();
    m_globalPermissions = calculateGlobalPermissions();
    m_readOnlyMode = qnCommon->isReadOnly();

    for(const QnResourcePtr &resource: qnResPool->getResources())
        updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    connect(resource, &QnResource::parentIdChanged,    this, &QnWorkbenchAccessController::updatePermissions);

    if (const QnLayoutResourcePtr &layout = resource.dynamicCast<QnLayoutResource>())
    {
        connect(layout, &QnLayoutResource::lockedChanged,       this, &QnWorkbenchAccessController::updatePermissions);
    }

    if (const QnUserResourcePtr& user = resource.dynamicCast<QnUserResource>())
    {
        connect(user, &QnUserResource::permissionsChanged,  this, &QnWorkbenchAccessController::updatePermissions);
        connect(user, &QnUserResource::userGroupChanged,    this, &QnWorkbenchAccessController::updatePermissions);
    }

    updatePermissions(resource);
}

void QnWorkbenchAccessController::at_resourcePool_resourceRemoved(const QnResourcePtr &resource)
{
    disconnect(resource, NULL, this, NULL);

    setPermissionsInternal(resource, Qn::NoPermissions); /* So that the signal is emitted. */
    m_dataByResource.remove(resource);
}

void QnWorkbenchAccessController::at_accessibleResourcesChanged(const QnUuid& userId)
{
    if (m_user && m_user->getId() == userId)
        recalculateAllPermissions();
}
