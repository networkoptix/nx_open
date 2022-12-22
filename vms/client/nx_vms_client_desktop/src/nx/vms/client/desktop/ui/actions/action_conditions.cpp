// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_conditions.h"

#include <QtWidgets/QAction>

#include <camera/camera_data_manager.h>
#include <camera/loaders/caching_camera_data_loader.h>
#include <camera/resource_display.h>
#include <client/client_module.h>
#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_controller_pool.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/fake_media_server.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/providers/resource_access_provider.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <network/router.h>
#include <network/system_helpers.h>
#include <nx/build_info.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/condition/generic_condition.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/desktop/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/desktop/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/desktop/joystick/settings/manager.h>
#include <nx/vms/client/desktop/network/cloud_url_validator.h>
#include <nx/vms/client/desktop/radass/radass_support.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/resource_descriptor.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/virtual_camera_manager.h>
#include <nx/vms/client/desktop/utils/virtual_camera_state.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/utils/platform/autorun.h>
#include <plugins/resource/desktop_camera/desktop_resource_base.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <ui/dialogs/ptz_manage_dialog.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/camera/camera_replacement.h>
#include <watchers/cloud_status_watcher.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

namespace {

/**
 * Condition which is a conjunction of two conditions.
 * It acts like logical AND, e.g. an action is enabled if the all conditions in the conjunction is true.
 * But the result (ActionVisibility) may have 3 values: [Invisible, Disabled, Enabled], so this action condition chooses
 * the minimal value from its conjuncts.
 */
class ConjunctionCondition: public Condition
{
public:
    ConjunctionCondition(ConditionWrapper&& l, ConditionWrapper&& r):
        l(std::move(l)),
        r(std::move(r))
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        // A bit of laziness.
        auto first = l->check(parameters, context);
        if (first == InvisibleAction)
            return first;
        return qMin(first, r->check(parameters, context));
    }

private:
    ConditionWrapper l;
    ConditionWrapper r;
};

/**
 * Condition which is a disjunction of two conditions.
 * It acts like logical OR, e.g. an action is enabled if one of conditions in the conjunction is true.
 * But the result (ActionVisibility) may have 3 values: [Invisible, Disabled, Enabled], so this action condition chooses
 * the maximal value from its conjuncts.
 */
class DisjunctionCondition: public Condition
{
public:
    DisjunctionCondition(ConditionWrapper&& l, ConditionWrapper&& r):
        l(std::move(l)),
        r(std::move(r))
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        // A bit of laziness.
        auto first = l->check(parameters, context);
        if (first == EnabledAction)
            return first;
        return qMax(first, r->check(parameters, context));
    }

private:
    ConditionWrapper l;
    ConditionWrapper r;
};

class NegativeCondition: public Condition
{
public:
    NegativeCondition(ConditionWrapper&& condition):
        m_condition(std::move(condition))
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        return m_condition->check(parameters, context) == InvisibleAction
            ? EnabledAction
            : InvisibleAction;
    }

private:
    ConditionWrapper m_condition;
};

class ScopedCondition: public Condition
{
public:
    ScopedCondition(ActionScope scope, ConditionWrapper&& condition):
        m_scope(scope),
        m_condition(std::move(condition))
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        if (parameters.scope() != m_scope)
            return EnabledAction;
        return m_condition->check(parameters, context);
    }

private:
    ActionScope m_scope;
    ConditionWrapper m_condition;
};

class CurrentLayoutFitsCondition: public Condition
{
public:
    CurrentLayoutFitsCondition(ConditionWrapper&& condition):
        m_condition(std::move(condition))
    {
    }

    virtual ActionVisibility check(
        const Parameters& /*parameters*/,
        QnWorkbenchContext* context) override
    {
        auto currentLayout = context->workbench()->currentLayout();
        if (!currentLayout || !currentLayout->resource())
            return InvisibleAction;

        return m_condition->check(currentLayout->resource(), context);
    }

private:
    ConditionWrapper m_condition;
};

class CustomBoolCondition: public Condition
{
public:
    using CheckDelegate = std::function<bool(
        const Parameters& parameters, QnWorkbenchContext* context)>;

    CustomBoolCondition(CheckDelegate delegate):
        m_delegate(delegate)
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        return m_delegate(parameters, context)
            ? EnabledAction
            : InvisibleAction;
    }

private:
    CheckDelegate m_delegate;
};

class CustomCondition: public Condition
{
public:
    using CheckDelegate = std::function<ActionVisibility(
        const Parameters& parameters, QnWorkbenchContext* context)>;

    CustomCondition(CheckDelegate delegate):
        m_delegate(delegate)
    {
    }

    virtual ActionVisibility check(const Parameters& parameters, QnWorkbenchContext* context) override
    {
        return m_delegate(parameters, context);
    }

private:
    CheckDelegate m_delegate;
};

class VirtualCameraCondition: public Condition
{
public:
    using CheckDelegate = std::function<ActionVisibility(const VirtualCameraState& state)>;

    VirtualCameraCondition(CheckDelegate delegate):
        m_delegate(delegate)
    {
        NX_ASSERT(m_delegate);
    }

    virtual ActionVisibility check(
        const Parameters& parameters, QnWorkbenchContext* /*context*/) override
    {
        const auto camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
        if (!camera || !camera->hasFlags(Qn::virtual_camera))
            return InvisibleAction;

        auto systemContext = SystemContext::fromResource(camera);
        if (!systemContext->virtualCameraManager())
            return InvisibleAction;

        const auto state = systemContext->virtualCameraManager()->state(camera);
        return m_delegate(state);
    }

private:
    CheckDelegate m_delegate;
};

TimePeriodType periodType(const QnTimePeriod& period)
{
    if (period.isNull())
        return NullTimePeriod;

    if (period.isEmpty())
        return EmptyTimePeriod;

    return NormalTimePeriod;
}

bool canExportPeriods(
    const QnResourceList& resources,
    const QnTimePeriod& period,
    bool ignoreLoadedChunks = false)
{
    return std::any_of(
        resources.cbegin(),
        resources.cend(),
        [&](const QnResourcePtr& resource)
        {
            const auto media = resource.dynamicCast<QnMediaResource>();
            if (!media)
                return false;

            if (resource->hasFlags(Qn::still_image))
                return false;

            auto systemContext = SystemContext::fromResource(resource);
            if (!NX_ASSERT(systemContext))
                return false;

            const auto accessController = systemContext->accessController();

            if (!accessController->hasPermissions(resource, Qn::ExportPermission))
                return false;

            const auto isAviFile = resource->hasFlags(Qn::local_video)
                && !resource->hasFlags(Qn::periods);
            if (isAviFile)
                return true;

            // We may not have loaded chunks when using Bookmarks export for camera, which was not
            // opened yet. This is no important as bookmarks are automatically deleted when archive
            // is not available for the given period.
            if (ignoreLoadedChunks)
                return true;

            // This condition can be checked in the bookmarks dialog when loader is not created.
            const auto loader =
                systemContext->cameraDataManager()->loader(media, /*createIfNotExists*/ true);
            return loader && loader->periods(Qn::RecordingContent).intersects(period);
        });
}

bool canExportBookmarkInternal(const QnCameraBookmark& bookmark, QnWorkbenchContext* context)
{
    const QnTimePeriod period(bookmark.startTimeMs, bookmark.durationMs);
    if (periodType(period) != NormalTimePeriod)
        return false;

    const QnResourceList resources{
        context->resourcePool()->getResourceById(bookmark.cameraId)
    };
    return canExportPeriods(resources, period, /*ignoreLoadedChunks*/ true);
}

bool resourceHasVideo(const QnResourcePtr& resource)
{
    if (!resource->hasFlags(Qn::video))
        return false;

    if (const auto mediaResource = resource.dynamicCast<QnMediaResource>())
        return mediaResource->hasVideo();

    return false;
}

} // namespace

Condition::Condition()
{
}

Condition::~Condition()
{
}

ActionVisibility Condition::check(const QnResourceList& /*resources*/, QnWorkbenchContext* /*context*/)
{
    return InvisibleAction;
}

ActionVisibility Condition::check(const LayoutItemIndexList& layoutItems, QnWorkbenchContext* context)
{
    return check(ParameterTypes::resources(layoutItems), context);
}

ActionVisibility Condition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context)
{
    return check(ParameterTypes::layoutItems(widgets), context);
}

ActionVisibility Condition::check(const QnWorkbenchLayoutList& layouts, QnWorkbenchContext* context)
{
    return check(ParameterTypes::resources(layouts), context);
}

ActionVisibility Condition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    switch (parameters.type())
    {
        case ResourceType:
            return check(parameters.resources(), context);
        case WidgetType:
            return check(parameters.widgets(), context);
        case LayoutType:
            return check(parameters.layouts(), context);
        case LayoutItemType:
            return check(parameters.layoutItems(), context);
        default:
            NX_ASSERT(false, nx::format("Invalid parameter type '%1'.").arg(parameters.items().typeName()));
            return InvisibleAction;
    }
}

ConditionWrapper::ConditionWrapper()
{
}

ConditionWrapper::ConditionWrapper(Condition* condition):
    m_condition(condition)
{
}

Condition* ConditionWrapper::operator->() const
{
    return m_condition.get();
}

ConditionWrapper::operator bool() const
{
    return m_condition.get() != nullptr;
}

ConditionWrapper operator&&(ConditionWrapper&& l, ConditionWrapper&& r)
{
    return ConditionWrapper(new ConjunctionCondition(std::move(l), std::move(r)));
}

ConditionWrapper operator||(ConditionWrapper&& l, ConditionWrapper&& r)
{
    return ConditionWrapper(new DisjunctionCondition(std::move(l), std::move(r)));
}

ConditionWrapper operator!(ConditionWrapper&& l)
{
    return ConditionWrapper(new NegativeCondition(std::move(l)));
}

ResourceCondition::ResourceCondition(ResourceCondition::CheckDelegate delegate,
    MatchMode matchMode)
    :
    m_delegate(delegate),
    m_matchMode(matchMode)
{
}

ActionVisibility ResourceCondition::check(const QnResourceList& resources,
    QnWorkbenchContext* /*context*/)
{
    return GenericCondition::check<QnResourcePtr>(resources, m_matchMode,
        [this](const QnResourcePtr& resource) { return m_delegate(resource); })
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility ResourceCondition::check(const QnResourceWidgetList& widgets,
    QnWorkbenchContext* /*context*/)
{
    return GenericCondition::check<QnResourceWidget*>(widgets, m_matchMode,
        [this](QnResourceWidget* widget)
        {
            if (auto resource = ParameterTypes::resource(widget))
                return m_delegate(resource);
            return false;
        })
        ? EnabledAction
        : InvisibleAction;
}

ConditionWrapper PreventWhenFullscreenTransition::condition()
{
    return new PreventWhenFullscreenTransition();
}

ActionVisibility PreventWhenFullscreenTransition::check(
    const Parameters& parameters,
    QnWorkbenchContext* context)
{
    return !nx::build_info::isMacOsX() || context->mainWindow()->updatesEnabled()
        ? EnabledAction
        : DisabledAction;
}

bool VideoWallReviewModeCondition::isVideoWallReviewMode(QnWorkbenchContext* context) const
{
    return context->workbench()->currentLayout()->isVideoWallReviewLayout();
}

ActionVisibility VideoWallReviewModeCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    return isVideoWallReviewMode(context)
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility RequiresOwnerCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    if (context->user() && context->user()->isOwner())
        return EnabledAction;
    return InvisibleAction;
}

ItemZoomedCondition::ItemZoomedCondition(bool requiredZoomedState):
    m_requiredZoomedState(requiredZoomedState)
{
}

ActionVisibility ItemZoomedCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context)
{
    if (widgets.size() != 1 || !widgets[0])
        return InvisibleAction;

    const auto widget = widgets[0];
    if (widget->resource()->hasFlags(Qn::videowall))
        return InvisibleAction;

    return ((widget->item() == context->workbench()->item(Qn::ZoomedRole)) == m_requiredZoomedState)
        ? EnabledAction
        : InvisibleAction;
}

SmartSearchCondition::SmartSearchCondition(bool requiredGridDisplayValue):
    m_hasRequiredGridDisplayValue(true),
    m_requiredGridDisplayValue(requiredGridDisplayValue)
{
}

SmartSearchCondition::SmartSearchCondition():
    m_hasRequiredGridDisplayValue(false),
    m_requiredGridDisplayValue(false)
{
}

ActionVisibility SmartSearchCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    for (auto widget: widgets)
    {
        if (!widget)
            continue;

        if (!widget->resource()->hasFlags(Qn::motion))
            continue;

        if (widget->isZoomWindow())
            continue;

        if (!resourceHasVideo(widget->resource()))
            continue;

        if (m_hasRequiredGridDisplayValue)
        {
            if (static_cast<bool>(widget->options() & QnResourceWidget::DisplayMotion) == m_requiredGridDisplayValue)
                return EnabledAction;
        }
        else
        {
            return EnabledAction;
        }
    }

    return InvisibleAction;
}

ActionVisibility DisplayInfoCondition::check(
    const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    const bool enabled = std::any_of(widgets.begin(), widgets.end(),
        [](auto widget) { return widget->visibleButtons() & Qn::InfoButton; });
    return enabled ? EnabledAction : InvisibleAction;
}

ActionVisibility ClearMotionSelectionCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    bool hasDisplayedGrid = false;

    for (auto widget: widgets)
    {
        if (!widget)
            continue;

        if (widget->options() & QnResourceWidget::DisplayMotion)
        {
            hasDisplayedGrid = true;

            if (auto mediaWidget = dynamic_cast<const QnMediaResourceWidget*>(widget))
                foreach(const QRegion &region, mediaWidget->motionSelection())
                if (!region.isEmpty())
                    return EnabledAction;
        }
    }

    return hasDisplayedGrid ? DisabledAction : InvisibleAction;
}

ActionVisibility ResourceRemovalCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    const auto nodeType = parameters.argument<ResourceTree::NodeType>(
        Qn::NodeTypeRole,
        ResourceTree::NodeType::resource);

    if (nodeType == ResourceTree::NodeType::sharedLayout
        || nodeType == ResourceTree::NodeType::sharedResource
        || nodeType == ResourceTree::NodeType::customResourceGroup)
    {
        return InvisibleAction;
    }

    QnUserResourcePtr owner = parameters.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    bool ownResources = owner && owner == context->user();

    auto canBeDeleted =
        [ownResources](const QnResourcePtr& resource)
        {
            NX_ASSERT(resource);
            if (!resource || !resource->resourcePool())
                return false;

            const auto layout = resource.dynamicCast<LayoutResource>();
            if (layout && layout->isIntercomLayout())
                return false;

            if (resource->hasFlags(Qn::layout) && !resource->hasFlags(Qn::local))
            {
                if (ownResources)
                    return true;

                // OK to remove autogenerated layout.
                QnLayoutResourcePtr layout = resource.dynamicCast<QnLayoutResource>();
                if (layout->isServiceLayout())
                    return true;

                // Cannot delete shared links on another user, they will be unshared instead.
                if (layout->isShared())
                    return false;

                return true;
            }

            if (resource->hasFlags(Qn::fake))
                return false;

            if (resource->hasFlags(Qn::cross_system))
                return false;

            if (resource->hasFlags(Qn::remote_server))
            {
                switch (resource->getStatus())
                {
                    case nx::vms::api::ResourceStatus::offline:
                    case nx::vms::api::ResourceStatus::unauthorized:
                    case nx::vms::api::ResourceStatus::mismatchedCertificate:
                        return true;
                    default:
                        return false;
                }
            }

            // Cannot remove local layout from server.
            if (resource.dynamicCast<QnFileLayoutResource>())
                return false;

            if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
            {
                // Cannot remove edge camera from it's own server when it is offline.
                // TODO: #ynikitenkov Use edge server tracker here.
                const QnMediaServerResourcePtr parentServer = camera->getParentServer();
                if (parentServer
                    && QnMediaServerResource::isHiddenEdgeServer(parentServer)
                    && parentServer->getStatus() == nx::vms::api::ResourceStatus::online
                    && !camera->hasFlags(Qn::virtual_camera))
                {
                    return false;
                }
            }

            // All other resources can be safely deleted if we have correct permissions.
            return true;
        };

    auto resources = parameters.resources();
    return std::any_of(resources.cbegin(), resources.cend(), canBeDeleted)
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility StopSharingCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    const auto nodeType = parameters.argument<ResourceTree::NodeType>(Qn::NodeTypeRole,
        ResourceTree::NodeType::resource);
    if (nodeType != ResourceTree::NodeType::sharedLayout)
        return InvisibleAction;

    auto user = parameters.argument<QnUserResourcePtr>(Qn::UserResourceRole);
    auto roleId = parameters.argument<QnUuid>(Qn::UuidRole);
    NX_ASSERT(user || !roleId.isNull());
    if (!user && roleId.isNull())
        return InvisibleAction;

    QnResourceAccessSubject subject = user
        ? QnResourceAccessSubject(user)
        : QnResourceAccessSubject(context->userRolesManager()->userRole(roleId));
    if (!subject.isValid())
        return InvisibleAction;

    for (auto resource : parameters.resources())
    {
        if (context->resourceAccessProvider()->accessibleVia(subject, resource)
            == nx::core::access::Source::shared)
        {
            return EnabledAction;
        }
    }

    return DisabledAction;
}

ActionVisibility RenameResourceCondition::check(const Parameters& parameters, QnWorkbenchContext* /*context*/)
{
    const auto nodeType = parameters.argument<ResourceTree::NodeType>(Qn::NodeTypeRole,
        ResourceTree::NodeType::resource);

    switch (nodeType)
    {
        case ResourceTree::NodeType::resource:
        case ResourceTree::NodeType::sharedLayout:
        case ResourceTree::NodeType::sharedResource:
        {
            if (parameters.resources().size() != 1)
                return InvisibleAction;

            QnResourcePtr target = parameters.resource();
            if (!target)
                return InvisibleAction;

            /* Renaming users directly from resource tree is disabled due do digest re-generation need. */
            if (target->hasFlags(Qn::user))
                return InvisibleAction;

            // According to the specification cross system camera resources mustn't have 'rename'
            // context menu action.
            if (target->hasFlags(Qn::cross_system) && !target->hasFlags(Qn::layout))
                return InvisibleAction;

            /* Edge servers renaming is forbidden. */
            if (QnMediaServerResource::isEdgeServer(target))
                return InvisibleAction;

            /* Incompatible resources cannot be renamed */
            if (target.dynamicCast<QnFakeMediaServerResource>())
                return InvisibleAction;

            return EnabledAction;
        }
        case ResourceTree::NodeType::edge:
        case ResourceTree::NodeType::recorder:
            return EnabledAction;
        default:
            break;
    }

    return InvisibleAction;
}

ActionVisibility LayoutItemRemovalCondition::check(
    const LayoutItemIndexList& layoutItems,
    QnWorkbenchContext* context)
{
    for (const LayoutItemIndex& item: layoutItems)
    {
        if (!ResourceAccessManager::hasPermissions(item.layout(),
            Qn::WritePermission | Qn::AddRemoveItemsPermission))
        {
            return InvisibleAction;
        }

        const auto resourceId = item.layout()->getItem(item.uuid()).resource.id;
        const auto resource =
            context->resourcePool()->getResourceById<QnResource>(resourceId);

        if (nx::vms::common::isIntercomOnIntercomLayout(resource, item.layout()))
        {
            const QnUuid intercomToDeleteId = resource->getId();

            QSet<QnUuid> intercomLayoutItemIds; // Other intercom item copies on the layout.

            const auto intercomLayoutItems = item.layout()->getItems();
            for (const QnLayoutItemData& intercomLayoutItem: intercomLayoutItems)
            {
                const auto itemResourceId = intercomLayoutItem.resource.id;
                if (itemResourceId == intercomToDeleteId && intercomLayoutItem.uuid != item.uuid())
                    intercomLayoutItemIds.insert(intercomLayoutItem.uuid);
            }

            for (const LayoutItemIndex& item: layoutItems)
                intercomLayoutItemIds.remove(item.uuid());

            // If all intercom copies on the layout is selected - removal is forbidden.
            if (intercomLayoutItemIds.isEmpty())
                return InvisibleAction;
        }
    }

    return EnabledAction;
}

SaveLayoutCondition::SaveLayoutCondition(bool isCurrent):
    m_current(isCurrent)
{
}

ActionVisibility SaveLayoutCondition::check(
    const QnResourceList& resources,
    QnWorkbenchContext* context)
{
    LayoutResourcePtr layout;

    if (m_current)
    {
        layout = context->workbench()->currentLayout()->resource();
    }
    else
    {
        if (resources.size() != 1)
            return InvisibleAction;

        layout = resources[0].dynamicCast<LayoutResource>();
    }

    if (!layout)
        return InvisibleAction;

    if (layout->data().contains(Qn::VideoWallResourceRole))
        return InvisibleAction;

    if (layout->data().contains(Qn::LayoutTourUuidRole))
        return InvisibleAction;

    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return DisabledAction;

    return systemContext->layoutSnapshotManager()->isSaveable(layout)
        ? EnabledAction
        : DisabledAction;
}

LayoutCountCondition::LayoutCountCondition(int minimalRequiredCount):
    m_minimalRequiredCount(minimalRequiredCount)
{
}

ActionVisibility LayoutCountCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    if (context->workbench()->layouts().size() < m_minimalRequiredCount)
        return DisabledAction;
    return EnabledAction;
}

ActionVisibility TakeScreenshotCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    if (widgets.size() != 1)
        return InvisibleAction;

    auto widget = widgets[0];
    if (widget->resource()->flags() & (Qn::still_image | Qn::server))
        return InvisibleAction;

    Qn::RenderStatus renderStatus = widget->renderStatus();
    if (renderStatus == Qn::NothingRendered || renderStatus == Qn::CannotRender)
        return DisabledAction;

    return EnabledAction;
}

ActionVisibility AdjustVideoCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    if (widgets.size() != 1)
        return InvisibleAction;

    const auto widget = widgets[0];
    const auto resource = widget ? widget->resource() : QnResourcePtr();
    NX_ASSERT(widget && resource);

    if ((resource->flags() & (Qn::server | Qn::videowall | Qn::layout))
        || (resource->hasFlags(Qn::web_page)))
    {
        return InvisibleAction;
    }

    const QString url = resource->getUrl().toLower();
    if (resource->hasFlags(Qn::still_image) && !url.endsWith(".jpg") && !url.endsWith(".jpeg"))
        return InvisibleAction;

    if (!resourceHasVideo(resource))
        return InvisibleAction;

    Qn::RenderStatus renderStatus = widget->renderStatus();
    if (renderStatus == Qn::NothingRendered || renderStatus == Qn::CannotRender)
        return DisabledAction;

    return EnabledAction;
}

TimePeriodCondition::TimePeriodCondition(TimePeriodTypes periodTypes, ActionVisibility nonMatchingVisibility):
    m_periodTypes(periodTypes),
    m_nonMatchingVisibility(nonMatchingVisibility)
{
}

ActionVisibility TimePeriodCondition::check(const Parameters& parameters, QnWorkbenchContext* /*context*/)
{
    if (!parameters.hasArgument(Qn::TimePeriodRole))
        return InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (m_periodTypes.testFlag(periodType(period)))
        return EnabledAction;

    return m_nonMatchingVisibility;
}

ActionVisibility AddBookmarkCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (!parameters.hasArgument(Qn::TimePeriodRole))
        return InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (periodType(period) != NormalTimePeriod)
        return InvisibleAction;

    if (!context->workbench()->item(Qn::CentralRole))
        return InvisibleAction;

    QnResourcePtr resource = parameters.resource();
    if (!resource->flags().testFlag(Qn::live))
        return InvisibleAction;

    if (resource->flags().testFlag(Qn::sync))
    {
        QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsRole);
        if (!periods.intersects(period))
            return InvisibleAction;
    }

    return EnabledAction;
}

ActionVisibility ModifyBookmarkCondition::check(const Parameters& parameters, QnWorkbenchContext* /*context*/)
{
    if (!parameters.hasArgument(Qn::CameraBookmarkRole))
        return InvisibleAction;
    return EnabledAction;
}

ActionVisibility RemoveBookmarksCondition::check(const Parameters& parameters, QnWorkbenchContext* /*context*/)
{
    if (!parameters.hasArgument(Qn::CameraBookmarkListRole))
        return InvisibleAction;

    if (parameters.argument(Qn::CameraBookmarkListRole).value<QnCameraBookmarkList>().isEmpty())
        return InvisibleAction;

    return EnabledAction;
}

ActionVisibility PreviewCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    const auto resource = parameters.resource();
    const auto media = resource.dynamicCast<QnMediaResource>();
    if (!media)
        return InvisibleAction;

    if (resource->hasFlags(Qn::still_image))
        return InvisibleAction;

    if (auto camera = resource.dynamicCast<QnSecurityCamResource>())
    {
        if (camera->isDtsBased())
            return InvisibleAction;
    }

    const bool isPanoramic = media->getVideoLayout()->channelCount() > 1;
    if (isPanoramic)
        return InvisibleAction;

    if (parameters.scope() == SceneScope)
    {
        if (!context->workbench()->currentLayout()->isPreviewSearchLayout())
            return InvisibleAction;

        const auto widget = parameters.widget();
        NX_ASSERT(widget);
        const auto period = widget->item()->data(Qn::ItemSliderSelectionRole).value<QnTimePeriod>();
        const auto periods = widget->item()->data(Qn::TimePeriodsRole).value<QnTimePeriodList>();
        if (period.isEmpty() || periods.empty())
            return InvisibleAction;

       if (!periods.intersects(period))
           return InvisibleAction;

       return EnabledAction;
    }

    const auto containsAvailablePeriods = parameters.hasArgument(Qn::TimePeriodsRole);

    /// If parameters contain periods it means we need current selected item
    if (containsAvailablePeriods && !context->workbench()->item(Qn::CentralRole))
        return InvisibleAction;

    if (containsAvailablePeriods && resource->hasFlags(Qn::sync))
    {
        const auto period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
        const auto periods = parameters.argument<QnTimePeriodList>(Qn::TimePeriodsRole);
        if (!periods.intersects(period))
            return InvisibleAction;
    }

    return EnabledAction;
}

ActionVisibility StartCurrentLayoutTourCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    const auto tourId = context->workbench()->currentLayoutResource()->data()
        .value(Qn::LayoutTourUuidRole).value<QnUuid>();
    const auto tour = context->layoutTourManager()->tour(tourId);
    if (tour.isValid() && tour.items.size() > 0)
        return EnabledAction;
    return DisabledAction;
}

ActionVisibility ArchiveCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    if (resources.size() != 1)
        return InvisibleAction;

    bool hasFootage = resources[0]->flags().testFlag(Qn::video);
    return hasFootage
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility TimelineVisibleCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    return context->navigator()->isPlayingSupported()
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility ToggleTitleBarCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    return context->action(action::EffectiveMaximizeAction)->isChecked() ? EnabledAction : InvisibleAction;
}

ActionVisibility NoArchiveCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    return context->accessController()->hasGlobalPermission(GlobalPermission::viewArchive)
        ? InvisibleAction
        : EnabledAction;
}

ActionVisibility OpenInFolderCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    if (resources.size() != 1)
        return InvisibleAction;

    QnResourcePtr resource = resources[0];

    // Skip cameras inside the exported layouts.
    bool isLocalResource = resource->hasFlags(Qn::local_media) && resource->getParentId().isNull();

    bool isExportedLayout = resource->hasFlags(Qn::exported_layout);

    return isLocalResource || isExportedLayout ? EnabledAction : InvisibleAction;
}

ActionVisibility OpenInFolderCondition::check(const LayoutItemIndexList& layoutItems, QnWorkbenchContext* context)
{
    foreach(const LayoutItemIndex &index, layoutItems)
    {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems, context);
    }

    return InvisibleAction;
}

ActionVisibility LayoutSettingsCondition::check(const QnResourceList& resources, QnWorkbenchContext* context)
{
    if (resources.size() > 1)
        return InvisibleAction;

    QnResourcePtr resource;
    if (resources.size() > 0)
        resource = resources[0];
    else
        resource = context->workbench()->currentLayout()->resource();

    if (!resource)
        return InvisibleAction;

    if (!ResourceAccessManager::hasPermissions(resource, Qn::EditLayoutSettingsPermission))
        return InvisibleAction;
    return EnabledAction;
}

ActionVisibility CreateZoomWindowCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* context)
{
    if (widgets.size() != 1)
        return InvisibleAction;

    auto widget = dynamic_cast<QnMediaResourceWidget*>(widgets[0]);
    if (!widget)
        return InvisibleAction;

    if (!widget->hasVideo())
        return InvisibleAction;

    if (context->display()->zoomTargetWidget(widget))
        return InvisibleAction;

    return EnabledAction;
}

ActionVisibility NewUserLayoutCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (!parameters.hasArgument(Qn::NodeTypeRole))
        return InvisibleAction;

    const auto nodeType = parameters.argument(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
    const auto userResourceFromParameters =
        [parameters]
        {
            if (parameters.hasArgument(Qn::UserResourceRole))
                return parameters.argument(Qn::UserResourceRole).value<QnUserResourcePtr>();

            return parameters.size() > 0
                ? parameters.resource().dynamicCast<QnUserResource>()
                : QnUserResourcePtr{};
        };
    const auto user = userResourceFromParameters();

    /* Create layout for self. */
    if (nodeType == ResourceTree::NodeType::layouts)
        return EnabledAction;

    // No other nodes must provide a way to create own layout.
    if (!user || user == context->user())
        return InvisibleAction;

    // Create layout for the custom user, but not for role.
    if (nodeType == ResourceTree::NodeType::sharedLayouts && user)
        return EnabledAction;

    // Create layout for other user is allowed on this user's node.
    if (nodeType != ResourceTree::NodeType::resource)
        return InvisibleAction;

    nx::vms::api::LayoutData layoutData;
    layoutData.parentId = user->getId();
    return context->accessController()->canCreateLayout(layoutData)
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility OpenInLayoutCondition::check(
    const Parameters& parameters,
    QnWorkbenchContext* /*context*/)
{
    auto layout = parameters.argument<LayoutResourcePtr>(Qn::LayoutResourceRole);
    if (!NX_ASSERT(layout))
        return InvisibleAction;

    return canOpen(parameters.resources(), layout)
        ? EnabledAction
        : InvisibleAction;
}

bool OpenInLayoutCondition::canOpen(
    const QnResourceList& resources,
    const QnLayoutResourcePtr& layout) const
{
    auto getAccessController =
        [](const QnResourcePtr& resource) -> QnWorkbenchAccessController*
        {
            auto systemContext = SystemContext::fromResource(resource);
            if (NX_ASSERT(systemContext))
                return systemContext->accessController();
            return nullptr;
        };

    if (!layout)
    {
        return std::any_of(resources.cbegin(), resources.cend(),
            [getAccessController](const QnResourcePtr& resource)
            {
                if (resource->hasFlags(Qn::layout))
                    return true;

                auto accessController = getAccessController(resource);

                return QnResourceAccessFilter::isOpenableInLayout(resource)
                    && accessController
                    && accessController->hasPermissions(resource, Qn::ViewContentPermission);
            });
    }

    bool isExportedLayout = layout->isFile();

    for (const auto& resource: resources)
    {
        auto accessController = getAccessController(resource);
        const bool hasViewContentPermission = accessController
            && accessController->hasPermissions(resource, Qn::ViewContentPermission);

        if (!hasViewContentPermission || !QnResourceAccessFilter::isOpenableInLayout(resource))
            continue;

        /* Allow to duplicate items on the exported layout. */
        if (isExportedLayout && resource->getParentId() == layout->getId())
            return true;

        bool nonMedia = !resource->hasFlags(Qn::media);
        bool isLocalResource = resource->hasFlags(Qn::local_media);

        bool forbidden = isExportedLayout && (nonMedia || isLocalResource);
        if (!forbidden)
            return true;
    }

    return false;
}

ActionVisibility OpenInCurrentLayoutCondition::check(
    const QnResourceList& resources,
    QnWorkbenchContext* context)
{
    QnLayoutResourcePtr layout = context->workbench()->currentLayout()->resource();

    if (!layout)
        return InvisibleAction;

    return canOpen(resources, layout)
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility OpenInCurrentLayoutCondition::check(
    const Parameters& parameters,
    QnWorkbenchContext* context)
{
    /* Make sure we will get to specialized implementation */
    return Condition::check(parameters, context);
}

ActionVisibility OpenInNewEntityCondition::check(
    const QnResourceList& resources,
    QnWorkbenchContext* /*context*/)
{
    return canOpen(resources, QnLayoutResourcePtr())
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility OpenInNewEntityCondition::check(const LayoutItemIndexList& layoutItems, QnWorkbenchContext* context)
{
    for (const LayoutItemIndex& index: layoutItems)
    {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems, context);
    }

    return InvisibleAction;
}

ActionVisibility OpenInNewEntityCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    /* Make sure we will get to specialized implementation */
    return Condition::check(parameters, context);
}

ActionVisibility SetAsBackgroundCondition::check(const QnResourceList& resources, QnWorkbenchContext* context)
{
    if (resources.size() != 1)
        return InvisibleAction;
    QnResourcePtr resource = resources[0];
    if (!resource->hasFlags(Qn::url | Qn::local | Qn::still_image))
        return InvisibleAction;

    QnLayoutResourcePtr layout = context->workbench()->currentLayout()->resource();
    if (!layout)
        return InvisibleAction;

    if (layout->locked())
        return DisabledAction;
    return EnabledAction;
}

ActionVisibility SetAsBackgroundCondition::check(const LayoutItemIndexList& layoutItems, QnWorkbenchContext* context)
{
    for (const LayoutItemIndex& index: layoutItems)
    {
        QnLayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems, context);
    }

    return InvisibleAction;
}

ActionVisibility ChangeResolutionCondition::check(const Parameters& parameters,
    QnWorkbenchContext* context)
{
    const auto layout = context->workbench()->currentLayout()->resource();
    if (!layout)
        return InvisibleAction;

    std::vector<QnLayoutItemData> items;
    if (!parameters.layoutItems().empty())
    {
        for (const auto idx: parameters.layoutItems())
            items.push_back(idx.layout()->getItem(idx.uuid()));
    }
    else // Use all items from the current layout.
    {
        for (const auto& item: layout->getItems())
            items.push_back(item);
    }

    QnVirtualCameraResourceList cameras;
    for (const auto& item: items)
    {
        if (item.uuid.isNull())
            continue;

        // Skip zoom windows.
        if (!item.zoomTargetUuid.isNull())
            continue;

        auto resource = getResourceByDescriptor(item.resource);
        // Filter our non-camera items and I/O modules.
        if (auto camera = resource.dynamicCast<QnVirtualCameraResource>();
            camera && camera->hasVideo())
        {
            cameras.push_back(camera);
        }
    }

    if (cameras.empty())
        return InvisibleAction;

    const bool supported = isRadassSupported(cameras, MatchMode::any);
    return supported ? EnabledAction : DisabledAction;
}

PtzCondition::PtzCondition(Ptz::Capabilities capabilities, bool disableIfPtzDialogVisible):
    m_capabilities(capabilities),
    m_disableIfPtzDialogVisible(disableIfPtzDialogVisible)
{
}

ActionVisibility PtzCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    bool isPreviewSearchMode =
        parameters.scope() == SceneScope &&
        context->workbench()->currentLayout()->isPreviewSearchLayout();
    if (isPreviewSearchMode)
        return InvisibleAction;
    return Condition::check(parameters, context);
}

ActionVisibility PtzCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    foreach(const QnResourcePtr &resource, resources)
    {
        auto ptzPool = qnClientCoreModule->commonModule()->findInstance<QnPtzControllerPool>();
        if (!check(ptzPool->controller(resource)))
            return InvisibleAction;
    }

    if (m_disableIfPtzDialogVisible && QnPtzManageDialog::instance() && QnPtzManageDialog::instance()->isVisible())
        return DisabledAction;

    return EnabledAction;
}

ActionVisibility PtzCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    for (auto widget: widgets)
    {
        auto mediaWidget = dynamic_cast<const QnMediaResourceWidget*>(widget);
        if (!mediaWidget)
            return InvisibleAction;

        if (!check(mediaWidget->ptzController()))
            return InvisibleAction;

        if (mediaWidget->isZoomWindow())
            return InvisibleAction;
    }

    if (m_disableIfPtzDialogVisible && QnPtzManageDialog::instance() && QnPtzManageDialog::instance()->isVisible())
        return DisabledAction;

    return EnabledAction;
}

bool PtzCondition::check(const QnPtzControllerPtr &controller)
{
    return controller && controller->hasCapabilities(m_capabilities);
}

ActionVisibility NonEmptyVideowallCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    foreach(const QnResourcePtr &resource, resources)
    {
        if (!resource->hasFlags(Qn::videowall))
            continue;

        QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
        if (!videowall)
            continue;

        if (videowall->items()->getItems().isEmpty())
            continue;

        return EnabledAction;
    }
    return InvisibleAction;
}

SaveVideowallReviewCondition::SaveVideowallReviewCondition(bool isCurrent):
    m_current(isCurrent)
{
}

ActionVisibility SaveVideowallReviewCondition::check(
    const QnResourceList& resources,
    QnWorkbenchContext* context)
{
    LayoutResourceList layouts;

    if (m_current)
    {
        auto layout = context->workbench()->currentLayout()->resource();
        if (!layout)
            return InvisibleAction;

        if (context->workbench()->currentLayout()->isVideoWallReviewLayout())
            layouts << layout;
    }
    else
    {
        for (const QnResourcePtr& resource: resources)
        {
            if (!resource->hasFlags(Qn::videowall))
                continue;

            QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
            if (!videowall)
                continue;

            QnWorkbenchLayout* layout = QnWorkbenchLayout::instance(videowall);
            if (!layout)
                continue;

            layouts << layout->resource();
        }
    }

    if (layouts.isEmpty())
        return InvisibleAction;

    return EnabledAction;
}

ActionVisibility RunningVideowallCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    bool hasNonEmptyVideowall = false;
    foreach(const QnResourcePtr &resource, resources)
    {
        if (!resource->hasFlags(Qn::videowall))
            continue;

        QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
        if (!videowall)
            continue;

        if (videowall->items()->getItems().isEmpty())
            continue;

        hasNonEmptyVideowall = true;
        if (videowall->onlineItems().isEmpty())
            continue;

        return EnabledAction;
    }

    return hasNonEmptyVideowall
        ? DisabledAction
        : InvisibleAction;
}

ActionVisibility StartVideowallCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    QnUuid pcUuid = qnSettings->pcUuid();
    if (pcUuid.isNull())
        return InvisibleAction;

    bool hasAttachedItems = false;
    foreach(const QnResourcePtr &resource, resources)
    {
        if (!resource->hasFlags(Qn::videowall))
            continue;

        QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
        if (!videowall)
            continue;

        if (!videowall->pcs()->hasItem(pcUuid))
            continue;

        foreach(const QnVideoWallItem &item, videowall->items()->getItems())
        {
            if (item.pcUuid != pcUuid)
                continue;

            if (!item.runtimeStatus.online)
                return EnabledAction;

            hasAttachedItems = true;
        }
    }

    return hasAttachedItems
        ? DisabledAction
        : InvisibleAction;
}

ActionVisibility IdentifyVideoWallCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (parameters.videoWallItems().size() > 0)
    {
        // allow action if there is at least one online item
        foreach(const QnVideoWallItemIndex &index, parameters.videoWallItems())
        {
            if (!index.isValid())
                continue;
            if (index.item().runtimeStatus.online)
                return EnabledAction;
        }
        return InvisibleAction;
    }

    /* 'Identify' action should not be displayed as disabled anyway. */
    ActionVisibility baseResult = Condition::check(parameters, context);
    if (baseResult != EnabledAction)
        return InvisibleAction;
    return EnabledAction;
}

ActionVisibility DetachFromVideoWallCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (!context->user() || parameters.videoWallItems().isEmpty())
        return InvisibleAction;

    foreach(const QnVideoWallItemIndex &index, parameters.videoWallItems())
    {
        if (index.isNull())
            continue;
        if (!index.videowall()->items()->hasItem(index.uuid()))
            continue;
        if (index.videowall()->items()->getItem(index.uuid()).layout.isNull())
            continue;
        return EnabledAction;
    }
    return InvisibleAction;
}

ActionVisibility StartVideoWallControlCondition::check(const Parameters& parameters, QnWorkbenchContext* context)
{
    if (!context->user() || parameters.videoWallItems().isEmpty())
        return InvisibleAction;

    foreach(const QnVideoWallItemIndex &index, parameters.videoWallItems())
    {
        if (!index.isValid())
            continue;

        return EnabledAction;
    }
    return InvisibleAction;
}

ActionVisibility RotateItemCondition::check(const QnResourceWidgetList& widgets, QnWorkbenchContext* /*context*/)
{
    for (auto widget: widgets)
    {
        if (widget->options() & QnResourceWidget::WindowRotationForbidden)
            return InvisibleAction;
    }
    return EnabledAction;
}

LightModeCondition::LightModeCondition(Qn::LightModeFlags flags):
    m_lightModeFlags(flags)
{
}

ActionVisibility LightModeCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* /*context*/)
{
    if (qnRuntime->lightMode() & m_lightModeFlags)
        return InvisibleAction;
    return EnabledAction;
}

EdgeServerCondition::EdgeServerCondition(bool isEdgeServer):
    m_isEdgeServer(isEdgeServer)
{
}

ActionVisibility EdgeServerCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    foreach(const QnResourcePtr &resource, resources)
        if (m_isEdgeServer != QnMediaServerResource::isEdgeServer(resource))
            return InvisibleAction;
    return EnabledAction;
}

ResourceStatusCondition::ResourceStatusCondition(const QSet<nx::vms::api::ResourceStatus> statuses, bool allResources):
    m_statuses(statuses),
    m_all(allResources)
{
}

ResourceStatusCondition::ResourceStatusCondition(nx::vms::api::ResourceStatus status, bool allResources):
    ResourceStatusCondition(QSet<nx::vms::api::ResourceStatus>{status}, allResources)
{
}

ActionVisibility ResourceStatusCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    bool found = false;
    foreach(const QnResourcePtr &resource, resources)
    {
        if (!m_statuses.contains(resource->getStatus()))
        {
            if (m_all)
                return InvisibleAction;
        }
        else
        {
            found = true;
        }
    }
    return found ? EnabledAction : InvisibleAction;
}

ActionVisibility DesktopCameraCondition::check(const Parameters& /*parameters*/,
    QnWorkbenchContext* context)
{
    const auto screenRecordingAction = context->action(action::ToggleScreenRecordingAction);
    if (screenRecordingAction)
    {
        const auto user = context->user();
        if (!user)
            return InvisibleAction;

        const auto desktopCameraId = QnDesktopResource::calculateUniqueId(
            context->systemContext()->peerId(), user->getId());

        /* Do not check real pointer type to speed up check. */
        const auto desktopCamera =
            context->resourcePool()->getNetworkResourceByPhysicalId(
                desktopCameraId);
        if (desktopCamera && desktopCamera->hasFlags(Qn::desktop_camera))
            return EnabledAction;

        return DisabledAction;
    }

    // Screen recording is not supported
    return InvisibleAction;
}

ActionVisibility AutoStartAllowedCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* /*context*/)
{
    if (!nx::vms::utils::isAutoRunSupported())
        return InvisibleAction;
    return EnabledAction;
}

ItemsCountCondition::ItemsCountCondition(int count):
    m_count(count)
{
}

ActionVisibility ItemsCountCondition::check(const Parameters& /*parameters*/, QnWorkbenchContext* context)
{
    int count = context->workbench()->currentLayout()->items().size();

    return (m_count == MultipleItems && count > 1) || (m_count == count) ? EnabledAction : InvisibleAction;
}

ActionVisibility IoModuleCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    bool pureIoModules = std::all_of(resources.cbegin(), resources.cend(),
        [](const QnResourcePtr& resource)
        {
            if (!resource->hasFlags(Qn::io_module))
                return false; //quick check

            QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
            return mediaResource && !mediaResource->hasVideo(0);
        });

    return pureIoModules ? EnabledAction : InvisibleAction;
}

ActionVisibility MergeToCurrentSystemCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    if (resources.size() != 1)
        return InvisibleAction;

    QnMediaServerResourcePtr server = resources.first().dynamicCast<QnMediaServerResource>();
    if (!server)
        return InvisibleAction;

    const auto status = server->getStatus();
    if (status != nx::vms::api::ResourceStatus::incompatible
        && status != nx::vms::api::ResourceStatus::unauthorized
        && status != nx::vms::api::ResourceStatus::mismatchedCertificate)
    {
        return InvisibleAction;
    }

    if (helpers::serverBelongsToCurrentSystem(server))
        return InvisibleAction;

    return EnabledAction;
}

FakeServerCondition::FakeServerCondition(bool allResources):
    m_all(allResources)
{
}

ActionVisibility FakeServerCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    bool found = false;
    foreach(const QnResourcePtr &resource, resources)
    {
        QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        if (server.dynamicCast<QnFakeMediaServerResource>())
            found = true;
        else if (m_all)
            return InvisibleAction;
    }
    return found ? EnabledAction : InvisibleAction;
}

CloudServerCondition::CloudServerCondition(MatchMode matchMode):
    m_matchMode(matchMode)
{
}

ActionVisibility CloudServerCondition::check(const QnResourceList& resources, QnWorkbenchContext* /*context*/)
{
    auto isCloudResource = [](const QnResourcePtr& resource)
        {
            return isCloudServer(resource.dynamicCast<QnMediaServerResource>());
        };

    bool success = GenericCondition::check<QnResourcePtr>(resources, m_matchMode, isCloudResource);
    return success ? EnabledAction : InvisibleAction;
}

ActionVisibility ReachableServerCondition::check(
    const Parameters& parameters, QnWorkbenchContext* context)
{
    const auto server = parameters.resource().dynamicCast<QnMediaServerResource>();
    if (!server || server->hasFlags(Qn::fake))
        return InvisibleAction;

    const auto currentSession = qnClientCoreModule->networkModule()->session();

    if (!currentSession || server->getId() == currentSession->connection()->moduleInformation().id)
        return InvisibleAction;

    if (!appContext()->moduleDiscoveryManager()->getEndpoint(server->getId()))
        return DisabledAction;

    return EnabledAction;
}

ActionVisibility HideServersInTreeCondition::check(
    const Parameters& parameters, QnWorkbenchContext* context)
{
    const bool isLoggedIn = !context->accessController()->user().isNull();
    if (!isLoggedIn)
        return InvisibleAction;

    if (!parameters.hasArgument(Qn::NodeTypeRole))
        return InvisibleAction;

    const auto nodeType = parameters.argument(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
    if (nodeType == ResourceTree::NodeType::servers)
        return EnabledAction;

    const auto server = parameters.size() > 0
        ? parameters.resource().dynamicCast<QnMediaServerResource>()
        : QnMediaServerResourcePtr{};
    if (!server || server->hasFlags(Qn::fake))
        return InvisibleAction;

    const auto parentNodeType =
        parameters.argument(Qn::ParentNodeTypeRole).value<ResourceTree::NodeType>();
    if (parentNodeType == ResourceTree::NodeType::root)
        return EnabledAction;

    return InvisibleAction;
}

ActionVisibility ReplaceCameraCondition::check(const Parameters& parameters, QnWorkbenchContext*)
{
    using namespace nx::vms::common::utils;

    const auto resource = parameters.resource();

    if (!camera_replacement::cameraSupportsReplacement(resource))
        return InvisibleAction;

    if (!camera_replacement::cameraCanBeReplaced(resource))
        return DisabledAction;

    return EnabledAction;
}

//-------------------------------------------------------------------------------------------------
// Definitions of resource grouping related actions conditions.
//-------------------------------------------------------------------------------------------------

ActionVisibility CreateNewResourceTreeGroupCondition::check(
    const Parameters& parameters,
    QnWorkbenchContext* context)
{
    using namespace entity_resource_tree::resource_grouping;

    const auto resources = parameters.resources();
    if (resources.empty())
        return InvisibleAction;

    if (parameters.hasArgument(Qn::TopLevelParentNodeTypeRole))
    {
        const auto topLevelNodeType =
            parameters.argument(Qn::TopLevelParentNodeTypeRole).value<ResourceTree::NodeType>();
        if (topLevelNodeType != ResourceTree::NodeType::servers
            && topLevelNodeType != ResourceTree::NodeType::camerasAndDevices
            && topLevelNodeType != ResourceTree::NodeType::resource)
        {
            return InvisibleAction;
        }
    }

    const bool containCameras = std::any_of(std::cbegin(resources), std::cend(resources),
        [](const QnResourcePtr& resource)
        {
            return resource.dynamicCast<QnVirtualCameraResource>()
                || resource.dynamicCast<QnWebPageResource>();
        });

    if (!containCameras)
        return InvisibleAction;

    if (!parameters.argument(Qn::OnlyResourceTreeSiblingsRole).toBool())
        return DisabledAction;

    const bool containNonCameras = std::any_of(std::cbegin(resources), std::cend(resources),
        [](const QnResourcePtr& resource)
        {
            return !(resource.dynamicCast<QnVirtualCameraResource>() ||
                resource.dynamicCast<QnWebPageResource>());
        });

    if (containNonCameras)
        return DisabledAction;

    const auto groupId = resourceCustomGroupId(resources.first());
    if (compositeIdDimension(groupId) >= kUserDefinedGroupingDepth)
        return DisabledAction;

    return EnabledAction;
}

ActionVisibility AssignResourceTreeGroupIdCondition::check(
    const QnResourceList& resources,
    QnWorkbenchContext* context)
{
    return EnabledAction;
}

ActionVisibility MoveResourceTreeGroupIdCondition::check(
    const QnResourceList& resources,
    QnWorkbenchContext* context)
{
    return EnabledAction;
}

ActionVisibility RenameResourceTreeGroupCondition::check(
    const Parameters& parameters,
    QnWorkbenchContext* context)
{
    return parameters.argument(Qn::SelectedGroupIdsRole).toStringList().count() == 1
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility RemoveResourceTreeGroupCondition::check(
    const Parameters& parameters,
    QnWorkbenchContext* context)
{
    return parameters.argument(Qn::SelectedGroupFullyMatchesFilter).toBool()
        ? EnabledAction
        : DisabledAction;
}

namespace condition {

ConditionWrapper always()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* /*context*/)
        {
            return true;
        });
}

ConditionWrapper isTrue(bool value)
{
    return new CustomBoolCondition(
        [value](const Parameters& /*parameters*/, QnWorkbenchContext* /*context*/)
        {
            return value;
        });
}

ConditionWrapper isLoggedIn()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* context)
        {
            return !context->user().isNull();
        });
}

ConditionWrapper isLoggedInToCloud()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* /*context*/)
        {
            return appContext()->cloudStatusWatcher()->status()
                == QnCloudStatusWatcher::Status::Online;
        });
}

ConditionWrapper scoped(ActionScope scope, ConditionWrapper&& condition)
{
    return new ScopedCondition(scope, std::move(condition));
}

ConditionWrapper applyToCurrentLayout(ConditionWrapper&& condition)
{
    return new CurrentLayoutFitsCondition(std::move(condition));
}

ConditionWrapper hasGlobalPermission(GlobalPermission permission)
{
    return new CustomBoolCondition(
        [permission](const Parameters& /*parameters*/, QnWorkbenchContext* context)
        {
            return context->accessController()->hasGlobalPermission(permission);
        });
}

ConditionWrapper isPreviewSearchMode()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* context)
        {
            return parameters.scope() == SceneScope
                && context->workbench()->currentLayout()->isPreviewSearchLayout();
        });
}

ConditionWrapper hasFlags(Qn::ResourceFlags flags, MatchMode matchMode)
{
    return new ResourceCondition(
        [flags](const QnResourcePtr& resource)
        {
            return resource->hasFlags(flags);
        }, matchMode);
}

ConditionWrapper hasFlags(
    Qn::ResourceFlags includeFlags, Qn::ResourceFlags excludeFlags, MatchMode matchMode)
{
    return new ResourceCondition(
        [includeFlags, excludeFlags](const QnResourcePtr& resource)
        {
            const auto flags = resource->flags();
            return (flags & includeFlags) == includeFlags
                && (flags & excludeFlags) == 0;
        }, matchMode);
}

ConditionWrapper hasVideo(MatchMode matchMode, bool value)
{
    return new ResourceCondition(
        [value](const QnResourcePtr& resource)
        {
            return resourceHasVideo(resource) == value;
        }, matchMode);
}

ConditionWrapper treeNodeType(QSet<ResourceTree::NodeType> types)
{
    return new CustomBoolCondition(
        [types](const Parameters& parameters, QnWorkbenchContext* /*context*/)
        {
            // Actions, triggered manually or from scene, must not check node type
            return !parameters.hasArgument(Qn::NodeTypeRole)
                || types.contains(parameters.argument(Qn::NodeTypeRole).value<ResourceTree::NodeType>());
        });
}

ConditionWrapper isLayoutTourReviewMode()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* context)
        {
            return context->workbench()->currentLayout()->isShowreelReviewLayout();
        });
}

ConditionWrapper canSavePtzPosition()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* /*context*/)
        {
            auto widget = qobject_cast<const QnMediaResourceWidget*>(parameters.widget());
            NX_ASSERT(widget);
            if (!widget)
                return false;

            // Allow to save position on fisheye cameras only if dewarping is enabled.
            const bool isFisheyeCamera = widget->resource()->getDewarpingParams().enabled;
            if (isFisheyeCamera)
                return widget->item()->dewarpingParams().enabled;

            return true;
        });
}

ConditionWrapper hasTimePeriod()
{
    return new CustomCondition(
        [](const Parameters& parameters, QnWorkbenchContext* /*context*/)
        {
            if (!parameters.hasArgument(Qn::TimePeriodRole))
                return InvisibleAction;

            QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
            if (periodType(period) != NormalTimePeriod)
                return DisabledAction;

            QnTimePeriodList periods = parameters.argument<QnTimePeriodList>(Qn::MergedTimePeriodsRole);
            if (!periods.intersects(period))
                return DisabledAction;

            return EnabledAction;
        });
}

ConditionWrapper hasArgument(int key, int targetTypeId)
{
    return new CustomBoolCondition(
        [key, targetTypeId](const Parameters& parameters, QnWorkbenchContext* /*context*/)
        {
            if (!parameters.hasArgument(key))
                return false;

            return targetTypeId < 0 //< If targetTypeId < 0, type check is omitted.
                || parameters.argument(key).canConvert(targetTypeId);
        });
}

ConditionWrapper isAnalyticsEngine()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* /*context*/)
        {
            const auto& resouces = parameters.resources();

            return std::all_of(resouces.begin(), resouces.end(),
                [](const QnResourcePtr& resource)
                {
                    return resource.dynamicCast<nx::vms::common::AnalyticsEngineResource>();
                });
        });
}

ConditionWrapper syncIsForced()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* context)
        {
            return context->navigator()->syncIsForced();
        });
}

ConditionWrapper canExportLayout()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* context)
        {
            if (!parameters.hasArgument(Qn::TimePeriodRole))
                return false;

            const auto period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
            if (periodType(period) != NormalTimePeriod)
                return false;
            const auto resources = ParameterTypes::resources(context->display()->widgets());
            return canExportPeriods(resources, period);
        });
}


ConditionWrapper canExportBookmark()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* context)
        {
            if (!parameters.hasArgument(Qn::CameraBookmarkRole))
                return false;

            const auto bookmark = parameters.argument<QnCameraBookmark>(Qn::CameraBookmarkRole);
            return canExportBookmarkInternal(bookmark, context);
        });
}

ConditionWrapper canExportBookmarks()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* context)
        {
            if (!parameters.hasArgument(Qn::CameraBookmarkListRole))
                return false;

            const auto bookmarks =
                parameters.argument<QnCameraBookmarkList>(Qn::CameraBookmarkListRole);

            return std::any_of(bookmarks.cbegin(), bookmarks.cend(),
                [context](const QnCameraBookmark& bookmark)
                {
                    return canExportBookmarkInternal(bookmark, context);
                });
        });
}

ConditionWrapper virtualCameraUploadEnabled()
{
    return new VirtualCameraCondition(
        [](const VirtualCameraState& state)
        {
            return (state.status == VirtualCameraState::Unlocked)
                ? EnabledAction
                : InvisibleAction;
        });
}

ConditionWrapper canCancelVirtualCameraUpload()
{
    return new VirtualCameraCondition(
        [](const VirtualCameraState& state)
        {
            return state.isRunning()
                ? (state.isCancellable() ? EnabledAction : DisabledAction)
                : InvisibleAction;
        });
}

ConditionWrapper currentLayoutIsVideowallScreen()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* context)
        {
            const auto layout = context->workbench()->currentLayout();
            return layout && !layout->data(Qn::VideoWallItemGuidRole).value<QnUuid>().isNull();
        });
}

ConditionWrapper canForgetPassword()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* context)
        {
            const auto layout = parameters.resource().dynamicCast<QnFileLayoutResource>();
            return layout && layout->isEncrypted() && !layout->requiresPassword();
    });
}

ConditionWrapper canMakeShowreel()
{
    return new ResourceCondition(
        [](const QnResourcePtr& resource)
        {
            const auto layout = resource.dynamicCast<QnLayoutResource>();
            return layout && !layout::isEncrypted(layout) && !layout->hasFlags(Qn::cross_system);
        }, MatchMode::all);
}

ConditionWrapper isWorkbenchVisible()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* context)
        {
            return context->isWorkbenchVisible();
        });
}

ConditionWrapper hasSavedWindowsState()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* context)
        {
            return appContext()->clientStateHandler()->hasSavedConfiguration();
        });
}

ConditionWrapper hasOtherWindowsInSession()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* context)
        {
            return !appContext()->sharedMemoryManager()->isLastInstanceInCurrentSession();
        });
}

ConditionWrapper hasOtherWindows()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* context)
        {
            return appContext()->sharedMemoryManager()->runningInstancesIndices().size() > 1;
        });
}

ConditionWrapper hasNewEventRulesEngine()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, QnWorkbenchContext* context)
        {
            auto engine = appContext()->currentSystemContext()->vmsRulesEngine();
            return engine && engine->isEnabled();
        });
}

ConditionWrapper allowedToShowServersInResourceTree()
{
    return new CustomBoolCondition(
        [](const Parameters&, QnWorkbenchContext* context)
        {
            return context->accessController()->hasGlobalPermission(
                GlobalPermission::admin)
                || context->systemSettings()->showServersInTreeForNonAdmins();
        });
}

ConditionWrapper joystickConnected()
{
    return new CustomBoolCondition(
        [](const Parameters&, QnWorkbenchContext* context)
        {
            return !context->joystickManager()->devices().isEmpty();
        });
}

ConditionWrapper showBetaUpgradeWarning()
{
    return new CustomBoolCondition(
        [](const Parameters& /*params*/, QnWorkbenchContext* /*context*/)
        {
            return !nx::branding::isDesktopClientCustomized()
                && nx::branding::isDevCloudHost();
        });
}

ConditionWrapper isCloudSystemConnectionUserInteractionRequired()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext*)
        {
            const auto systemId = parameters.argument(Qn::CloudSystemIdRole).toString();
            const auto cloudContext =
                appContext()->cloudCrossSystemManager()->systemContext(systemId);

            return cloudContext && cloudContext->status()
                == CloudCrossSystemContext::Status::connectionFailure;
        }
    );
}

ConditionWrapper canSaveLayoutAs()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, QnWorkbenchContext* /*context*/)
        {
            auto resources = parameters.resources();
            if (resources.size() != 1)
                return false;

            auto layout = resources[0].dynamicCast<LayoutResource>();

            if (!layout)
                return false;

            if (layout->isServiceLayout())
                return false;

            if (layout->data().contains(Qn::VideoWallResourceRole))
                return false;

            /* Save as.. for exported layouts works very strange, disabling it for now. */
            if (layout->isFile())
                return false;

            return true;
        }
    );
}

} // namespace condition
} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
