// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_permissions_handler.h"

#include <QtWidgets/QAction>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>

namespace nx::vms::client::desktop {

namespace {

QSet<QnResourcePtr> localLayoutResources(const QnLayoutResourcePtr& layout)
{
    QnResourcePool* resourcePool = layout->resourcePool();

    QSet<QnResourcePtr> result;
    for (const auto& item: layout->getItems())
    {
        if (auto resource = resourcePool->getResourceByDescriptor(item.resource))
            result.insert(resource);
    }
    return result;
}

} // namespace

PermissionsHandler::PermissionsHandler(QObject* parent):
    QObject(parent),
    QnSessionAwareDelegate(parent)
{
    connect(action(ui::action::ShareLayoutAction), &QAction::triggered,
        this, &PermissionsHandler::at_shareLayoutAction_triggered);
    connect(action(ui::action::StopSharingLayoutAction), &QAction::triggered,
        this, &PermissionsHandler::at_stopSharingLayoutAction_triggered);
    connect(action(ui::action::ShareCameraAction), &QAction::triggered,
        this, &PermissionsHandler::at_shareCameraAction_triggered);
    connect(action(ui::action::ShareWebPageAction), &QAction::triggered,
        this, &PermissionsHandler::at_shareWebPageAction_triggered);
}

PermissionsHandler::~PermissionsHandler()
{
}

void PermissionsHandler::shareLayoutWith(const QnLayoutResourcePtr& layout,
    const QnResourceAccessSubject& subject)
{
    if (!NX_ASSERT(layout
        && subject.isValid()
        && !layout->isFile()
        && !layout->hasFlags(Qn::cross_system)))
    {
        return;
    }

    if (!layout->isShared())
        layout->setParentId(QnUuid());

    NX_ASSERT(layout->isShared());

    // If layout is changed, it will automatically be saved here (and become shared if needed).
    // Also we do not grant direct access to cameras anyway as layout will become shared
    // and do not ask confirmation, so we do not use common saveLayout() method anyway.
    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return;

    if (systemContext->layoutSnapshotManager()->save(layout))
        shareResourceWith(layout->getId(), subject);
}

void PermissionsHandler::shareCameraWith(const QnVirtualCameraResourcePtr& camera,
    const QnResourceAccessSubject& subject)
{
    if (NX_ASSERT(camera))
        shareResourceWith(camera->getId(), subject);
}

void PermissionsHandler::shareWebPageWith(const QnWebPageResourcePtr& webPage,
    const QnResourceAccessSubject& subject)
{
    if (NX_ASSERT(webPage))
        shareResourceWith(webPage->getId(), subject);
}

void PermissionsHandler::shareResourceWith(const QnUuid& resourceId,
    const QnResourceAccessSubject& subject)
{
    if (!NX_ASSERT(!resourceId.isNull() && subject.isValid()))
        return;

    if (resourceAccessManager()->hasGlobalPermission(subject, GlobalPermission::admin))
        return;

    auto accessible = sharedResourcesManager()->sharedResources(subject);
    if (accessible.contains(resourceId))
        return;

    accessible << resourceId;
    qnResourcesChangesManager->saveAccessibleResources(subject, accessible);
}

bool PermissionsHandler::confirmStopSharingLayouts(const QnResourceAccessSubject& subject,
    const QnLayoutResourceList& layouts)
{
    if (resourceAccessManager()->hasGlobalPermission(subject, GlobalPermission::accessAllMedia))
        return true;

    // Calculate all resources that were available through these layouts.
    // Check only Resources from the same System Context as the Layout. Cross-system Resources
    // cannot be available through Shared Layouts.
    QSet<QnUuid> layoutsIds;
    QSet<QnResourcePtr> resourcesOnLayouts;
    for (const auto& layout: layouts)
    {
        layoutsIds.insert(layout->getId());
        resourcesOnLayouts.unite(localLayoutResources(layout));
    }

    QnResourceList resourcesBecomeUnaccessible;
    for (const auto& resource: resourcesOnLayouts)
    {
        if (!QnResourceAccessFilter::isShareableMedia(resource))
            continue;

        QnResourceList providers;
        auto accessSource = resourceAccessProvider()->accessibleVia(subject, resource, &providers);
        if (accessSource != nx::core::access::Source::layout)
            continue;

        QSet<QnUuid> providerIds;
        for (const auto& provider: providers)
            providerIds << provider->getId();

        providerIds -= layoutsIds;

        /* This resource was available only via these layouts. */
        if (providerIds.isEmpty())
            resourcesBecomeUnaccessible << resource;
    }

    return ui::messages::Resources::stopSharingLayouts(mainWindowWidget(),
        resourcesBecomeUnaccessible, subject);
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void PermissionsHandler::at_shareLayoutAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    const auto layout = params.resource().dynamicCast<QnLayoutResource>();
    const auto user = params.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    const auto roleId = params.argument<QnUuid>(Qn::UuidRole);

    QnResourceAccessSubject subject = user
        ? QnResourceAccessSubject(user)
        : QnResourceAccessSubject(userRolesManager()->userRole(roleId));

    QnUserResourcePtr owner = layout->getParentResource().dynamicCast<QnUserResource>();
    if (owner && owner == user)
        return; /* Sharing layout with its owner does nothing. */

    /* Here layout will become shared, and owner will keep access rights. */
    if (owner && !layout->isShared())
        shareLayoutWith(layout, owner);

    shareLayoutWith(layout, subject);
}

void PermissionsHandler::at_stopSharingLayoutAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    const auto user = params.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    const auto roleId = params.argument<QnUuid>(Qn::UuidRole);
    NX_ASSERT(user || !roleId.isNull());
    if (!user && roleId.isNull())
        return;

    QnResourceAccessSubject subject = user
        ? QnResourceAccessSubject(user)
        : QnResourceAccessSubject(userRolesManager()->userRole(roleId));
    if (!subject.isValid())
        return;

    QnLayoutResourceList sharedLayouts;
    for (auto resource: params.resources().filtered<QnLayoutResource>())
    {
        if (resourceAccessProvider()->accessibleVia(subject, resource) == nx::core::access::Source::shared)
            sharedLayouts << resource;
    }
    if (sharedLayouts.isEmpty())
        return;

    if (!confirmStopSharingLayouts(subject, sharedLayouts))
        return;

    auto accessible = sharedResourcesManager()->sharedResources(subject);
    for (const auto& layout: sharedLayouts)
    {
        NX_ASSERT(!layout->isFile());
        accessible.remove(layout->getId());
    }
    qnResourcesChangesManager->saveAccessibleResources(subject, accessible);
}

void PermissionsHandler::at_shareCameraAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    const auto camera = params.resource().dynamicCast<QnVirtualCameraResource>();
    const auto user = params.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    const auto roleId = params.argument<QnUuid>(Qn::UuidRole);

    QnResourceAccessSubject subject = user
        ? QnResourceAccessSubject(user)
        : QnResourceAccessSubject(userRolesManager()->userRole(roleId));

    shareCameraWith(camera, subject);
}

void PermissionsHandler::at_shareWebPageAction_triggered()
{
    const auto params = menu()->currentParameters(sender());
    const auto webPage = params.resource().dynamicCast<QnWebPageResource>();
    const auto user = params.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    const auto roleId = params.argument<QnUuid>(Qn::UuidRole);

    QnResourceAccessSubject subject = user
        ? QnResourceAccessSubject(user)
        : QnResourceAccessSubject(userRolesManager()->userRole(roleId));

    shareWebPageWith(webPage, subject);
}

bool PermissionsHandler::tryClose(bool force)
{
    // do nothing
    return true;
}

void PermissionsHandler::forcedUpdate()
{
    //do nothing
}

} // namespace nx::vms::client::desktop
