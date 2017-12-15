#include "workbench_permissions_handler.h"

#include <QtWidgets/QAction>

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <api/app_server_connection.h>
#include <api/global_settings.h>

#include <client/client_globals.h>
#include <client/client_settings.h>
#include <client/client_message_processor.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource_management/resources_changes_manager.h>

#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/shared_resources_manager.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <nx/client/desktop/radass/radass_resource_manager.h>

#include <nx_ec/dummy_handler.h>
#include <nx_ec/managers/abstract_layout_manager.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <nx/client/desktop/ui/actions/action_parameter_types.h>
#include <ui/dialogs/layout_name_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/views/resource_list_view.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/handlers/workbench_videowall_handler.h>  // TODO: #GDM dependencies
#include <ui/workbench/workbench_state_manager.h>
#include <ui/workbench/extensions/workbench_layout_change_validator.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <nx/client/desktop/ui/messages/resources_messages.h>

#include <nx/utils/string.h>

#include <nx/utils/counter.h>
#include <utils/common/delete_later.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_value_rollback.h>
#include <nx/client/desktop/ui/workbench/layouts/layout_factory.h>

using boost::algorithm::any_of;
using boost::algorithm::all_of;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace workbench {

namespace {

QnResourceList calculateResourcesToShare(const QnResourceList& resources,
    const QnUserResourcePtr& user)
{
    auto sharingRequired = [user](const QnResourcePtr& resource)
        {
            if (!QnResourceAccessFilter::isShareableMedia(resource))
                return false;

            auto accessProvider = resource->commonModule()->resourceAccessProvider();
            return !accessProvider->hasAccess(user, resource);
        };
    return resources.filtered(sharingRequired);
}

} // namespace

PermissionsHandler::PermissionsHandler(QObject *parent):
    QObject(parent),
    QnSessionAwareDelegate(parent)
{
    connect(action(action::ShareLayoutAction),                   &QAction::triggered, this, &PermissionsHandler::at_shareLayoutAction_triggered);
    connect(action(action::StopSharingLayoutAction),             &QAction::triggered, this, &PermissionsHandler::at_stopSharingLayoutAction_triggered);
    connect(action(action::ShareCameraAction),                   &QAction::triggered, this, &PermissionsHandler::at_shareCameraAction_triggered);
}

PermissionsHandler::~PermissionsHandler()
{
}

void PermissionsHandler::shareLayoutWith(const QnLayoutResourcePtr &layout,
    const QnResourceAccessSubject &subject)
{
    NX_ASSERT(layout && subject.isValid());
    if (!layout || !subject.isValid())
        return;

    NX_ASSERT(!layout->isFile());
    if (layout->isFile())
        return;

    if (!layout->isShared())
        layout->setParentId(QnUuid());
    NX_ASSERT(layout->isShared());

    /* If layout is changed, it will automatically be saved here (and become shared if needed).
    * Also we do not grant direct access to cameras anyway as layout will become shared
    * and do not ask confirmation, so we do not use common saveLayout() method anyway. */
    if (!snapshotManager()->save(layout))
        return;

    /* Admins anyway have all shared layouts. */
    if (resourceAccessManager()->hasGlobalPermission(subject, Qn::GlobalAdminPermission))
        return;

    auto accessible = sharedResourcesManager()->sharedResources(subject);
    if (accessible.contains(layout->getId()))
        return;

    accessible << layout->getId();
    qnResourcesChangesManager->saveAccessibleResources(subject, accessible);
}

void PermissionsHandler::shareCameraWith(const QnVirtualCameraResourcePtr &camera,
    const QnResourceAccessSubject &subject)
{
    NX_ASSERT(camera && subject.isValid());
    if (!camera || !subject.isValid())
        return;

    /* Admins anyway have all shared layouts. */
    if (resourceAccessManager()->hasGlobalPermission(subject, Qn::GlobalAdminPermission))
        return;

    auto accessible = sharedResourcesManager()->sharedResources(subject);
    if (accessible.contains(camera->getId()))
        return;

    accessible << camera->getId();
    qnResourcesChangesManager->saveAccessibleResources(subject, accessible);
}

bool PermissionsHandler::confirmStopSharingLayouts(const QnResourceAccessSubject& subject,
    const QnLayoutResourceList& layouts)
{
    if (resourceAccessManager()->hasGlobalPermission(subject, Qn::GlobalAccessAllMediaPermission))
        return true;

    /* Calculate all resources that were available through these layouts. */
    QSet<QnUuid> layoutsIds;
    QSet<QnResourcePtr> resourcesOnLayouts;
    for (const auto& layout : layouts)
    {
        layoutsIds << layout->getId();
        resourcesOnLayouts += layout->layoutResources();
    }

    QnResourceList resourcesBecomeUnaccessible;
    for (const auto& resource : resourcesOnLayouts)
    {
        if (!QnResourceAccessFilter::isShareableMedia(resource))
            continue;

        QnResourceList providers;
        auto accessSource = resourceAccessProvider()->accessibleVia(subject, resource, &providers);
        if (accessSource != nx::core::access::Source::layout)
            continue;

        QSet<QnUuid> providerIds;
        for (const auto& provider : providers)
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
    auto params = menu()->currentParameters(sender());
    auto layout = params.resource().dynamicCast<QnLayoutResource>();
    auto user = params.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    auto roleId = params.argument<QnUuid>(Qn::UuidRole);

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
    auto params = menu()->currentParameters(sender());
    auto user = params.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    auto roleId = params.argument<QnUuid>(Qn::UuidRole);
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
    for (const auto& layout : sharedLayouts)
    {
        NX_ASSERT(!layout->isFile());
        accessible.remove(layout->getId());
    }
    qnResourcesChangesManager->saveAccessibleResources(subject, accessible);
}

void PermissionsHandler::at_shareCameraAction_triggered()
{
    auto params = menu()->currentParameters(sender());
    auto camera = params.resource().dynamicCast<QnVirtualCameraResource>();
    auto user = params.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    auto roleId = params.argument<QnUuid>(Qn::UuidRole);

    QnResourceAccessSubject subject = user
        ? QnResourceAccessSubject(user)
        : QnResourceAccessSubject(userRolesManager()->userRole(roleId));

    QnUserResourcePtr owner = camera->getParentResource().dynamicCast<QnUserResource>();
    if (owner && owner == user)
        return; /* Sharing layout with its owner does nothing. */

    /* Here layout will become shared, and owner will keep access rights. */
    //if (owner && !camera->isShared())
    //    shareCameraWith(layout, owner);

    shareCameraWith(camera, subject);
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

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
