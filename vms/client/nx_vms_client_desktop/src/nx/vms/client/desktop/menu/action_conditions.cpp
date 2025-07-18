// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_conditions.h"

#include <QtGui/QAction>

#include <camera/resource_display.h>
#include <client/client_module.h>
#include <client/client_runtime_settings.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_controller_pool.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <core/storage/file_storage/layout_storage_resource.h>
#include <network/router.h>
#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/client/core/camera/camera_data_manager.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_context.h>
#include <nx/vms/client/core/cross_system/cloud_cross_system_manager.h>
#include <nx/vms/client/core/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/resource/data_loaders/caching_camera_data_loader.h>
#include <nx/vms/client/core/resource/resource_descriptor_helpers.h>
#include <nx/vms/client/core/resource/screen_recording/desktop_resource.h>
#include <nx/vms/client/desktop/access/access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/condition/generic_condition.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/joystick/settings/manager.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/network/cloud_url_validator.h>
#include <nx/vms/client/desktop/radass/radass_support.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource_views/entity_resource_tree/resource_grouping/resource_grouping.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/virtual_camera/virtual_camera_manager.h>
#include <nx/vms/client/desktop/virtual_camera/virtual_camera_state.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/intercom/utils.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/utils/platform/autorun.h>
#include <recording/time_period.h>
#include <recording/time_period_list.h>
#include <ui/dialogs/ptz_manage_dialog.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/widgets/main_window.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/camera/camera_replacement.h>

#include "action_manager.h"

namespace nx::vms::client::desktop {
namespace menu {

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

    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override
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

    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override
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

    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override
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

    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override
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
        WindowContext* context) override
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
        const Parameters& parameters, WindowContext* context)>;

    CustomBoolCondition(CheckDelegate delegate):
        m_delegate(delegate)
    {
    }

    virtual ActionVisibility check(
        const Parameters& parameters,
        WindowContext* context) override
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
        const Parameters& parameters, WindowContext* context)>;

    CustomCondition(CheckDelegate delegate):
        m_delegate(delegate)
    {
    }

    virtual ActionVisibility check(
        const Parameters& parameters,
        WindowContext* context) override
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
        const Parameters& parameters, WindowContext* /*context*/) override
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

class ResourcesCondition: public Condition
{
public:
    using CheckDelegate = std::function<bool(const QnResourceList&, WindowContext*)>;

    ResourcesCondition(CheckDelegate delegate):
        m_delegate(std::move(delegate))
    {
        NX_ASSERT(m_delegate);
    }

    virtual ActionVisibility check(
        const QnResourceList& resources,
        WindowContext* context) override
    {
        return m_delegate(resources, context)
            ? EnabledAction
            : InvisibleAction;
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

bool canExportBookmarkInternal(const QnCameraBookmark& bookmark, WindowContext* context)
{
    const QnTimePeriod period(bookmark.startTimeMs, bookmark.durationMs);
    if (periodType(period) != NormalTimePeriod)
        return false;

    const QnResourceList resources{
        context->system()->resourcePool()->getResourceById(bookmark.cameraId)
    };
    return canExportPeriods(resources, period, /*ignoreLoadedChunks*/ true);
}

bool canCopyBookmarkToClipboardInternal(
    const QnCameraBookmark& bookmark, WindowContext* context)
{
    if (const auto& camera = context->system()->resourcePool()->getResourceById(bookmark.cameraId))
    {
        return context->system()->accessController()->
            hasPermissions(camera, Qn::ViewBookmarksPermission);
    }

    return false;
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

ActionVisibility Condition::check(const QnResourceList& /*resources*/, WindowContext* /*context*/)
{
    return InvisibleAction;
}

ActionVisibility Condition::check(const LayoutItemIndexList& layoutItems, WindowContext* context)
{
    return check(ParameterTypes::resources(layoutItems), context);
}

ActionVisibility Condition::check(const QnResourceWidgetList& widgets, WindowContext* context)
{
    return check(ParameterTypes::layoutItems(widgets), context);
}

ActionVisibility Condition::check(const QnWorkbenchLayoutList& layouts, WindowContext* context)
{
    return check(ParameterTypes::resources(layouts), context);
}

ActionVisibility Condition::check(const Parameters& parameters, WindowContext* context)
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
    WindowContext* /*context*/)
{
    return GenericCondition::check<QnResourcePtr>(resources, m_matchMode,
        [this](const QnResourcePtr& resource) { return m_delegate(resource); })
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility ResourceCondition::check(const QnResourceWidgetList& widgets,
    WindowContext* /*context*/)
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
    const Parameters& /*parameters*/,
    WindowContext* context)
{
    return !nx::build_info::isMacOsX() || context->mainWindowWidget()->updatesEnabled()
        ? EnabledAction
        : DisabledAction;
}

bool VideoWallReviewModeCondition::isVideoWallReviewMode(WindowContext* context) const
{
    return context->workbench()->currentLayout()->isVideoWallReviewLayout();
}

ActionVisibility VideoWallReviewModeCondition::check(const Parameters& /*parameters*/, WindowContext* context)
{
    return isVideoWallReviewMode(context)
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility RequiresAdministratorCondition::check(const Parameters& /*parameters*/, WindowContext* context)
{
    if (auto user = context->workbenchContext()->user(); user && user->isAdministrator())
        return EnabledAction;
    return InvisibleAction;
}

ItemZoomedCondition::ItemZoomedCondition(bool requiredZoomedState):
    m_requiredZoomedState(requiredZoomedState)
{
}

ActionVisibility ItemZoomedCondition::check(const QnResourceWidgetList& widgets, WindowContext* context)
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

ActionVisibility SmartSearchCondition::check(
    const QnResourceWidgetList& widgets,
    WindowContext* /*context*/)
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
    const QnResourceWidgetList& widgets, WindowContext* /*context*/)
{
    const bool enabled = std::any_of(widgets.begin(), widgets.end(),
        [](auto widget) { return widget->visibleButtons() & Qn::InfoButton; });
    return enabled ? EnabledAction : InvisibleAction;
}

ActionVisibility ClearMotionSelectionCondition::check(
    const QnResourceWidgetList& widgets,
    WindowContext* /*context*/)
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
            {
                for (const QRegion& region: mediaWidget->motionSelection())
                {
                    if (!region.isEmpty())
                        return EnabledAction;
                }
            }
        }
    }

    return hasDisplayedGrid ? DisabledAction : InvisibleAction;
}

ActionVisibility ResourceRemovalCondition::check(
    const Parameters& parameters,
    WindowContext* context)
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

    auto canBeDeleted =
        [](const QnResourcePtr& resource)
        {
            NX_ASSERT(resource);
            if (!resource || !resource->resourcePool())
                return false;

            const auto layout = resource.dynamicCast<core::LayoutResource>();
            if (layout && layout->layoutType() == core::LayoutResource::LayoutType::intercom)
                return false;

            if (resource->hasFlags(Qn::layout) && !resource->hasFlags(Qn::local))
                return true;

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

ActionVisibility RenameResourceCondition::check(const Parameters& parameters, WindowContext* /*context*/)
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
    WindowContext* context)
{
    for (const LayoutItemIndex& item: layoutItems)
    {
        if (!ResourceAccessManager::hasPermissions(item.layout(),
            Qn::WritePermission | Qn::AddRemoveItemsPermission))
        {
            return InvisibleAction;
        }

        if (!item.layout()->systemContext())
            continue;

        const auto resourceId = item.layout()->getItem(item.uuid()).resource.id;
        const auto resource =
            item.layout()->resourcePool()->getResourceById<QnResource>(resourceId);

        if (nx::vms::common::isIntercomOnIntercomLayout(resource, item.layout()))
        {
            const nx::Uuid intercomToDeleteId = resource->getId();

            QSet<nx::Uuid> intercomLayoutItemIds; // Other intercom item copies on the layout.

            const auto intercomLayoutItems = item.layout()->getItems();
            for (const common::LayoutItemData& intercomLayoutItem: intercomLayoutItems)
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
    WindowContext* context)
{
    core::LayoutResourcePtr layout;

    if (m_current)
    {
        layout = context->workbench()->currentLayout()->resource();
    }
    else
    {
        if (resources.size() != 1)
            return InvisibleAction;

        layout = resources[0].dynamicCast<core::LayoutResource>();
    }

    if (!layout)
        return InvisibleAction;

    if (layout->data().contains(Qn::VideoWallResourceRole))
        return InvisibleAction;

    if (layout->data().contains(Qn::ShowreelUuidRole))
        return InvisibleAction;

    auto systemContext = SystemContext::fromResource(layout);
    if (!NX_ASSERT(systemContext))
        return DisabledAction;

    return layout->canBeSaved() ? EnabledAction : DisabledAction;
}

LayoutCountCondition::LayoutCountCondition(int minimalRequiredCount):
    m_minimalRequiredCount(minimalRequiredCount)
{
}

ActionVisibility LayoutCountCondition::check(const Parameters& /*parameters*/, WindowContext* context)
{
    if (context->workbench()->layouts().size() < m_minimalRequiredCount)
        return DisabledAction;
    return EnabledAction;
}

ActionVisibility TakeScreenshotCondition::check(const QnResourceWidgetList& widgets, WindowContext* /*context*/)
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

ActionVisibility AdjustVideoCondition::check(const QnResourceWidgetList& widgets, WindowContext* /*context*/)
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

ActionVisibility TimePeriodCondition::check(const Parameters& parameters, WindowContext* /*context*/)
{
    if (!parameters.hasArgument(Qn::TimePeriodRole))
        return InvisibleAction;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    if (m_periodTypes.testFlag(periodType(period)))
        return EnabledAction;

    return m_nonMatchingVisibility;
}

ActionVisibility AddBookmarkCondition::check(const Parameters& parameters, WindowContext* context)
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

ActionVisibility ModifyBookmarkCondition::check(const Parameters& parameters, WindowContext* /*context*/)
{
    if (!parameters.hasArgument(core::CameraBookmarkRole))
        return InvisibleAction;
    return EnabledAction;
}

ActionVisibility RemoveBookmarksCondition::check(const Parameters& parameters, WindowContext* /*context*/)
{
    if (!parameters.hasArgument(Qn::CameraBookmarkListRole))
        return InvisibleAction;

    if (parameters.argument(Qn::CameraBookmarkListRole).value<QnCameraBookmarkList>().empty())
        return InvisibleAction;

    return EnabledAction;
}

ActionVisibility PreviewCondition::check(const Parameters& parameters, WindowContext* context)
{
    const auto resource = parameters.resource();
    const auto media = resource.dynamicCast<QnMediaResource>();
    if (!media)
        return InvisibleAction;

    if (resource->hasFlags(Qn::still_image))
        return InvisibleAction;

    if (auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        if (camera->isDtsBased() || !camera->hasVideo())
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

ActionVisibility StartCurrentShowreelCondition::check(
    const Parameters& /*parameters*/,
    WindowContext* context)
{
    auto currentLayout = context->workbench()->currentLayoutResource();
    const auto showreelId = currentLayout->data().value(Qn::ShowreelUuidRole).value<nx::Uuid>();
    const auto showreel = context->system()->showreelManager()->showreel(showreelId);
    if (showreel.isValid() && showreel.items.size() > 0)
        return EnabledAction;
    return DisabledAction;
}

ActionVisibility ArchiveCondition::check(const QnResourceList& resources, WindowContext* /*context*/)
{
    if (resources.size() != 1)
        return InvisibleAction;

    bool hasFootage = resources[0]->flags().testFlag(Qn::video);
    return hasFootage
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility TimelineVisibleCondition::check(const Parameters& /*parameters*/, WindowContext* context)
{
    return context->workbenchContext()->navigator()->isPlayingSupported()
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility ToggleTitleBarCondition::check(const Parameters& /*parameters*/, WindowContext* context)
{
    return context->menu()->action(menu::EffectiveMaximizeAction)->isChecked()
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility NoArchiveCondition::check(
    const QnResourceList& resources,
    WindowContext* /*context*/)
{
    const bool noResouceWithArchiveAccess = std::none_of(resources.begin(), resources.end(),
        [](auto resource)
        {
            return ResourceAccessManager::hasPermissions(resource,
                Qn::Permission::ViewFootagePermission);
        });

    return noResouceWithArchiveAccess ? EnabledAction : InvisibleAction;
}

ActionVisibility OpenInFolderCondition::check(const QnResourceList& resources, WindowContext* /*context*/)
{
    if (resources.size() != 1)
        return InvisibleAction;

    QnResourcePtr resource = resources[0];

    // Skip cameras inside the exported layouts.
    bool isLocalResource = resource->hasFlags(Qn::local_media) && resource->getParentId().isNull();

    bool isExportedLayout = resource->hasFlags(Qn::exported_layout);

    return isLocalResource || isExportedLayout ? EnabledAction : InvisibleAction;
}

ActionVisibility OpenInFolderCondition::check(const LayoutItemIndexList& layoutItems, WindowContext* context)
{
    foreach(const LayoutItemIndex &index, layoutItems)
    {
        common::LayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems, context);
    }

    return InvisibleAction;
}

ActionVisibility LayoutSettingsCondition::check(const QnResourceList& resources, WindowContext* context)
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

ActionVisibility CreateZoomWindowCondition::check(const QnResourceWidgetList& widgets, WindowContext* context)
{
    if (widgets.size() != 1)
        return InvisibleAction;

    auto widget = dynamic_cast<QnMediaResourceWidget*>(widgets[0]);
    if (!widget)
        return InvisibleAction;

    if (!widget->hasVideo())
        return InvisibleAction;

    if (context->workbenchContext()->display()->zoomTargetWidget(widget))
        return InvisibleAction;

    return EnabledAction;
}

ActionVisibility NewUserLayoutCondition::check(const Parameters& parameters, WindowContext* context)
{
    if (!parameters.hasArgument(Qn::NodeTypeRole))
        return InvisibleAction;

    const auto nodeType = parameters.argument(Qn::NodeTypeRole).value<ResourceTree::NodeType>();

    /* Create layout for self. */
    if (nodeType == ResourceTree::NodeType::layouts)
        return EnabledAction;

    return InvisibleAction;
}

ActionVisibility OpenInLayoutCondition::check(
    const Parameters& parameters,
    WindowContext* /*context*/)
{
    auto layout = parameters.argument<core::LayoutResourcePtr>(core::LayoutResourceRole);
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
        [](const QnResourcePtr& resource) -> nx::vms::client::core::AccessController*
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
    WindowContext* context)
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
    WindowContext* context)
{
    /* Make sure we will get to specialized implementation */
    return Condition::check(parameters, context);
}

ActionVisibility OpenInNewEntityCondition::check(
    const QnResourceList& resources,
    WindowContext* /*context*/)
{
    return canOpen(resources, QnLayoutResourcePtr())
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility OpenInNewEntityCondition::check(
    const LayoutItemIndexList& layoutItems,
    WindowContext* context)
{
    for (const LayoutItemIndex& index: layoutItems)
    {
        common::LayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems, context);
    }

    return InvisibleAction;
}

ActionVisibility OpenInNewEntityCondition::check(
    const Parameters& parameters,
    WindowContext* context)
{
    /* Make sure we will get to specialized implementation */
    return Condition::check(parameters, context);
}

ActionVisibility SetAsBackgroundCondition::check(
    const QnResourceList& resources,
    WindowContext* context)
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

ActionVisibility SetAsBackgroundCondition::check(
    const LayoutItemIndexList& layoutItems,
    WindowContext* context)
{
    for (const LayoutItemIndex& index: layoutItems)
    {
        common::LayoutItemData itemData = index.layout()->getItem(index.uuid());
        if (itemData.zoomRect.isNull())
            return Condition::check(layoutItems, context);
    }

    return InvisibleAction;
}

ActionVisibility ChangeResolutionCondition::check(const Parameters& parameters,
    WindowContext* context)
{
    const auto layout = context->workbench()->currentLayout()->resource();
    if (!layout)
        return InvisibleAction;

    std::vector<common::LayoutItemData> items;
    if (!parameters.layoutItems().empty())
    {
        for (const auto& idx: parameters.layoutItems())
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

        auto resource = core::getResourceByDescriptor(item.resource);
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

ActionVisibility PtzCondition::check(const Parameters& parameters, WindowContext* context)
{
    bool isPreviewSearchMode =
        parameters.scope() == SceneScope &&
        context->workbench()->currentLayout()->isPreviewSearchLayout();
    if (isPreviewSearchMode)
        return InvisibleAction;
    return Condition::check(parameters, context);
}

ActionVisibility PtzCondition::check(
    const QnResourceList& resources,
    WindowContext* /*context*/)
{
    for (const QnResourcePtr& resource: resources)
    {
        auto systemContext = SystemContext::fromResource(resource);
        if (!NX_ASSERT(systemContext))
            return InvisibleAction;

        auto ptzPool = systemContext->ptzControllerPool();
        if (!checkInternal(ptzPool->controller(resource)))
            return InvisibleAction;
    }

    const auto ptzManagementDialog = QnPtzManageDialog::instance();
    if (m_disableIfPtzDialogVisible && ptzManagementDialog && ptzManagementDialog->isVisible())
        return DisabledAction;

    return EnabledAction;
}

ActionVisibility PtzCondition::check(
    const QnResourceWidgetList& widgets,
    WindowContext* /*context*/)
{
    for (const auto& widget: widgets)
    {
        auto mediaWidget = dynamic_cast<const QnMediaResourceWidget*>(widget);
        if (!mediaWidget)
            return InvisibleAction;

        if (!checkInternal(mediaWidget->ptzController()))
            return InvisibleAction;

        if (mediaWidget->isZoomWindow())
            return InvisibleAction;
    }

    const auto ptzManagementDialog = QnPtzManageDialog::instance();
    if (m_disableIfPtzDialogVisible && ptzManagementDialog && ptzManagementDialog->isVisible())
        return DisabledAction;

    return EnabledAction;
}

bool PtzCondition::checkInternal(const QnPtzControllerPtr& controller)
{
    return controller && controller->hasCapabilities(m_capabilities);
}

ActionVisibility NonEmptyVideowallCondition::check(
    const QnResourceList& resources,
    WindowContext* /*context*/)
{
    for (const QnResourcePtr& resource: resources)
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
    WindowContext* context)
{
    core::LayoutResourceList layouts;

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

ActionVisibility StartVideowallCondition::check(
    const QnResourceList& resources,
    WindowContext* /*context*/)
{
    nx::Uuid pcUuid = appContext()->localSettings()->pcUuid();
    if (pcUuid.isNull())
        return InvisibleAction;

    bool hasAttachedItems = false;
    for (const QnResourcePtr& resource: resources)
    {
        if (!resource->hasFlags(Qn::videowall))
            continue;

        QnVideoWallResourcePtr videowall = resource.dynamicCast<QnVideoWallResource>();
        if (!videowall)
            continue;

        if (!videowall->pcs()->hasItem(pcUuid))
            continue;

        for (const QnVideoWallItem& item: videowall->items()->getItems())
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

ActionVisibility IdentifyVideoWallCondition::check(
    const Parameters& parameters,
    WindowContext* context)
{
    if (parameters.videoWallItems().size() > 0)
    {
        // allow action if there is at least one online item
        for (const QnVideoWallItemIndex& index: parameters.videoWallItems())
        {
            if (!index.isValid())
                continue;

            if (index.item().runtimeStatus.online)
                return EnabledAction;
        }
        return InvisibleAction;
    }

    /* 'Identify' action should not be displayed as disabled anyway. */
    ActionVisibility baseResult = condition::videowallIsRunning()->check(parameters, context);
    if (baseResult != EnabledAction)
        return InvisibleAction;

    return EnabledAction;
}

ActionVisibility DetachFromVideoWallCondition::check(const Parameters& parameters, WindowContext* context)
{
    if (!context->workbenchContext()->user() || parameters.videoWallItems().isEmpty())
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

ActionVisibility StartVideoWallControlCondition::check(const Parameters& parameters, WindowContext* context)
{
    if (!context->workbenchContext()->user() || parameters.videoWallItems().isEmpty())
        return InvisibleAction;

    foreach(const QnVideoWallItemIndex &index, parameters.videoWallItems())
    {
        if (!index.isValid())
            continue;

        return EnabledAction;
    }
    return InvisibleAction;
}

ActionVisibility RotateItemCondition::check(const QnResourceWidgetList& widgets, WindowContext* /*context*/)
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

ActionVisibility LightModeCondition::check(const Parameters& /*parameters*/, WindowContext* /*context*/)
{
    if (qnRuntime->lightMode() & m_lightModeFlags)
        return InvisibleAction;
    return EnabledAction;
}

EdgeServerCondition::EdgeServerCondition(bool isEdgeServer):
    m_isEdgeServer(isEdgeServer)
{
}

ActionVisibility EdgeServerCondition::check(const QnResourceList& resources, WindowContext* /*context*/)
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

ActionVisibility ResourceStatusCondition::check(const QnResourceList& resources, WindowContext* /*context*/)
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

ActionVisibility AutoStartAllowedCondition::check(const Parameters& /*parameters*/, WindowContext* /*context*/)
{
    if (!nx::vms::utils::isAutoRunSupported())
        return InvisibleAction;
    return EnabledAction;
}

ItemsCountCondition::ItemsCountCondition(int count):
    m_count(count)
{
}

ActionVisibility ItemsCountCondition::check(const Parameters& /*parameters*/, WindowContext* context)
{
    int count = context->workbench()->currentLayout()->items().size();

    return (m_count == MultipleItems && count > 1) || (m_count == count) ? EnabledAction : InvisibleAction;
}

ActionVisibility IoModuleCondition::check(const QnResourceList& resources, WindowContext* /*context*/)
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

CloudServerCondition::CloudServerCondition(MatchMode matchMode):
    m_matchMode(matchMode)
{
}

ActionVisibility CloudServerCondition::check(const QnResourceList& resources, WindowContext* /*context*/)
{
    auto isCloudResource = [](const QnResourcePtr& resource)
        {
            return isCloudServer(resource.dynamicCast<QnMediaServerResource>());
        };

    bool success = GenericCondition::check<QnResourcePtr>(resources, m_matchMode, isCloudResource);
    return success ? EnabledAction : InvisibleAction;
}

ActionVisibility ReachableServerCondition::check(
    const Parameters& parameters, WindowContext* context)
{
    if (!context->system()->accessController()->hasPowerUserPermissions()
        && !context->system()->globalSettings()->showServersInTreeForNonAdmins())
    {
        return InvisibleAction;
    }

    const auto server = parameters.resource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return InvisibleAction;

    const auto currentSession = appContext()->networkModule()->session();

    if (!currentSession || server->getId() == currentSession->connection()->moduleInformation().id)
        return InvisibleAction;

    if (!appContext()->moduleDiscoveryManager()->getEndpoint(server->getId()))
        return DisabledAction;

    return EnabledAction;
}

ActionVisibility HideServersInTreeCondition::check(
    const Parameters& parameters, WindowContext* context)
{
    const bool isLoggedIn = !context->system()->accessController()->user().isNull();
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
    if (!server)
        return InvisibleAction;

    const auto parentNodeType =
        parameters.argument(Qn::ParentNodeTypeRole).value<ResourceTree::NodeType>();
    if (parentNodeType == ResourceTree::NodeType::root)
        return EnabledAction;

    return InvisibleAction;
}

ActionVisibility ToggleProxiedResourcesCondition::check(
    const Parameters& parameters,
    WindowContext* context)
{
    const bool isLoggedIn = !context->system()->accessController()->user().isNull();
    if (!isLoggedIn)
        return InvisibleAction;

    if (!parameters.hasArgument(Qn::NodeTypeRole))
        return InvisibleAction;

    const auto nodeType = parameters.argument(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
    if (nodeType == ResourceTree::NodeType::servers
        || nodeType == ResourceTree::NodeType::camerasAndDevices)
    {
        return EnabledAction;
    }

    const auto server = parameters.size() > 0
        ? parameters.resource().dynamicCast<QnMediaServerResource>()
        : QnMediaServerResourcePtr{};

    if (!server)
        return InvisibleAction;

    const auto parentNodeType =
        parameters.argument(Qn::ParentNodeTypeRole).value<ResourceTree::NodeType>();

    if (parentNodeType == ResourceTree::NodeType::root)
        return EnabledAction;

    return InvisibleAction;
}

ActionVisibility ReplaceCameraCondition::check(const Parameters& parameters, WindowContext*)
{
    using namespace nx::vms::common::utils;

    const auto resource = parameters.resource();

    if (!camera_replacement::cameraSupportsReplacement(resource))
        return InvisibleAction;

    if (!camera_replacement::cameraCanBeReplaced(resource))
        return DisabledAction;

    return EnabledAction;
}

ItemMuteActionCondition::ItemMuteActionCondition(bool mute):
    m_mute(mute)
{
}

ActionVisibility ItemMuteActionCondition::check(const Parameters& parameters, WindowContext*)
{
    int mutedCount = 0;
    int unmutedCount = 0;
    for (QnResourceWidget* widget: parameters.widgets())
    {
        auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
        if (mediaWidget && mediaWidget->canBeMuted())
            (mediaWidget->isMuted() ? mutedCount : unmutedCount)++;
    }

    if (m_mute)
    {
        return unmutedCount > 0 ? EnabledAction : InvisibleAction;
    }
    else
    {
        return unmutedCount == 0 && mutedCount > 0 ? EnabledAction : InvisibleAction;
    }
}


//-------------------------------------------------------------------------------------------------
// Definitions of resource grouping related actions conditions.
//-------------------------------------------------------------------------------------------------

ActionVisibility CreateNewResourceTreeGroupCondition::check(
    const Parameters& parameters,
    WindowContext* /*context*/)
{
    using namespace entity_resource_tree::resource_grouping;

    const auto resources = parameters.resources();
    if (resources.empty())
        return InvisibleAction;

    if (parameters.hasArgument(Qn::TopLevelParentNodeTypeRole))
    {
        std::set<ResourceTree::NodeType> topLevelTreeNodesThatAllowsCustomGroups{
            ResourceTree::NodeType::servers,
            ResourceTree::NodeType::camerasAndDevices,
            ResourceTree::NodeType::resource
        };

        if (ini().foldersForLayoutsInResourceTree)
            topLevelTreeNodesThatAllowsCustomGroups.insert(ResourceTree::NodeType::layouts);

        const auto topLevelNodeType = parameters.argument(Qn::TopLevelParentNodeTypeRole)
            .value<ResourceTree::NodeType>();

        if (!topLevelTreeNodesThatAllowsCustomGroups.contains(topLevelNodeType))
            return InvisibleAction;
    }

    const auto resourceCanBeInCustomGroup =
        [](const QnResourcePtr& resource)
        {
            return resource.dynamicCast<QnVirtualCameraResource>()
                || resource.dynamicCast<QnWebPageResource>()
                || (resource.dynamicCast<QnLayoutResource>()
                    && ini().foldersForLayoutsInResourceTree);
        };

    // None of selected resources can be placed into a group.
    if (!std::any_of(std::cbegin(resources), std::cend(resources), resourceCanBeInCustomGroup))
        return InvisibleAction;

    // Selection contains type of resource that cannot be placed into a group.
    if (!std::all_of(std::cbegin(resources), std::cend(resources), resourceCanBeInCustomGroup))
        return DisabledAction;

    if (!parameters.argument(Qn::OnlyResourceTreeSiblingsRole).toBool())
        return DisabledAction;

    const auto groupId = resourceCustomGroupId(resources.first());
    if (compositeIdDimension(groupId) >= kUserDefinedGroupingDepth)
        return DisabledAction;

    return EnabledAction;
}

ActionVisibility AssignResourceTreeGroupIdCondition::check(
    const QnResourceList& /*resources*/,
    WindowContext* /*context*/)
{
    return EnabledAction;
}

ActionVisibility MoveResourceTreeGroupIdCondition::check(
    const QnResourceList& /*resources*/,
    WindowContext* /*context*/)
{
    return EnabledAction;
}

ActionVisibility RenameResourceTreeGroupCondition::check(
    const Parameters& parameters,
    WindowContext* /*context*/)
{
    return parameters.argument(Qn::SelectedGroupIdsRole).toStringList().count() == 1
        ? EnabledAction
        : InvisibleAction;
}

ActionVisibility RemoveResourceTreeGroupCondition::check(
    const Parameters& parameters,
    WindowContext* /*context*/)
{
    return parameters.argument(Qn::SelectedGroupFullyMatchesFilter).toBool()
        ? EnabledAction
        : DisabledAction;
}

namespace condition {

ConditionWrapper always()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* /*context*/)
        {
            return true;
        });
}

ConditionWrapper isTrue(bool value)
{
    return new CustomBoolCondition(
        [value](const Parameters& /*parameters*/, WindowContext* /*context*/)
        {
            return value;
        });
}

ConditionWrapper isLoggedIn(ActionVisibility defaultVisibility)
{
    return new CustomCondition(
        [defaultVisibility](const Parameters& /*parameters*/, WindowContext* context)
        {
            return context->workbenchContext()->user().isNull()
                ? defaultVisibility
                : EnabledAction;
        });
}

ConditionWrapper hasVideoWallFeature()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            return context->system()->saasServiceManager()->hasFeature(
                nx::vms::api::SaasTierLimitName::videowallEnabled);
        });
}

ConditionWrapper isLoggedInAsCloudUser()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            return context->workbenchContext()->user()
                && context->workbenchContext()->user()->isCloud();
        });
}

ConditionWrapper isLoggedInToCloud()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* /*context*/)
        {
            return appContext()->cloudStatusWatcher()->status()
                == core::CloudStatusWatcher::Status::Online;
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
        [permission](const Parameters& /*parameters*/, WindowContext* context)
        {
            return context->system()->accessController()->hasGlobalPermissions(permission);
        });
}

ConditionWrapper isPreviewSearchMode()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* context)
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
        [types](const Parameters& parameters, WindowContext* /*context*/)
        {
            // Actions, triggered manually or from scene, must not check node type
            return !parameters.hasArgument(Qn::NodeTypeRole)
                || types.contains(parameters.argument(Qn::NodeTypeRole).value<ResourceTree::NodeType>());
        });
}

ConditionWrapper parentTreeNodeType(ResourceTree::NodeType type)
{
    return new CustomBoolCondition(
        [type](const Parameters& parameters, WindowContext* /*context*/)
        {
            if (!parameters.hasArgument(Qn::ParentNodeTypeRole))
                return false;

            return type
                == parameters.argument(Qn::ParentNodeTypeRole).value<ResourceTree::NodeType>();
        });
}

ConditionWrapper isShowreelReviewMode()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            return context->workbench()->currentLayout()->isShowreelReviewLayout();
        });
}

ConditionWrapper layoutIsLocked()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            return context->workbench()->currentLayoutResource()->locked();
        });
}

ConditionWrapper selectedItemsContainLockedLayout()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* context)
        {
            const auto videoWallItemIndexes = parameters.videoWallItems();
            for (const QnVideoWallItemIndex& index: videoWallItemIndexes)
            {
                if (!index.isValid())
                    continue;

                QnVideoWallItem existingItem = index.item();
                const auto layout = context->system()->resourcePool()
                    ->getResourceById<QnLayoutResource>(existingItem.layout);
                if (layout && layout->locked())
                    return true;
            }

            return false;
        });
}

ConditionWrapper canSavePtzPosition()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* /*context*/)
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
        [](const Parameters& parameters, WindowContext* /*context*/)
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
        [key, targetTypeId](const Parameters& parameters, WindowContext* /*context*/)
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
        [](const Parameters& parameters, WindowContext* /*context*/)
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
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            return context->workbenchContext()->navigator()->syncIsForced();
        });
}

ConditionWrapper canExportLayout()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* context)
        {
            if (!parameters.hasArgument(Qn::TimePeriodRole))
                return false;

            const auto period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
            if (periodType(period) != NormalTimePeriod)
                return false;
            const auto resources = ParameterTypes::resources(
                context->workbenchContext()->display()->widgets());
            return canExportPeriods(resources, period);
        });
}


ConditionWrapper canExportBookmark()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* context)
        {
            if (!parameters.hasArgument(core::CameraBookmarkRole))
                return false;

            const auto bookmark = parameters.argument<QnCameraBookmark>(core::CameraBookmarkRole);
            return canExportBookmarkInternal(bookmark, context);
        });
}

ConditionWrapper canExportBookmarks()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* context)
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

ConditionWrapper canCopyBookmarkToClipboard()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* context)
        {
            if (!parameters.hasArgument(core::CameraBookmarkRole))
                return false;

            const auto bookmark = parameters.argument<QnCameraBookmark>(core::CameraBookmarkRole);
            return canCopyBookmarkToClipboardInternal(bookmark, context);
        });
}

ConditionWrapper canCopyBookmarksToClipboard()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* context)
        {
            if (!parameters.hasArgument(Qn::CameraBookmarkListRole))
                return false;

            const auto bookmarks =
                parameters.argument<QnCameraBookmarkList>(Qn::CameraBookmarkListRole);

            return std::any_of(bookmarks.cbegin(), bookmarks.cend(),
                [context](const QnCameraBookmark& bookmark)
                {
                    return canCopyBookmarkToClipboardInternal(bookmark, context);
                });
        });
}

ConditionWrapper isDeviceAccessRelevant(nx::vms::api::AccessRights requiredAccessRights)
{
    return new CustomBoolCondition(
        [requiredAccessRights](const Parameters& /*parameters*/, WindowContext* context)
        {
            return context->system()->accessController()->isDeviceAccessRelevant(
                requiredAccessRights);
        });
}

ConditionWrapper hasRemoteArchiveCapability()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            return GenericCondition::check<QnVirtualCameraResourcePtr>(
                context->system()->resourcePool()->getAllCameras(
                    /*serverId*/ Uuid{},
                    /*ignoreDesktopCameras*/ true),
                MatchMode::any,
                [](const QnVirtualCameraResourcePtr& camera)
                {
                    return camera->hasCameraCapabilities(api::DeviceCapability::remoteArchive);
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
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            const auto layout = context->workbench()->currentLayout();
            return layout && !layout->data(Qn::VideoWallItemGuidRole).value<nx::Uuid>().isNull();
        });
}

ConditionWrapper canForgetPassword()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* /*context*/)
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
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            return context->workbenchContext()->isWorkbenchVisible();
        });
}

ConditionWrapper hasSavedWindowsState()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* /*context*/)
        {
            return appContext()->clientStateHandler()->hasSavedConfiguration();
        });
}

ConditionWrapper hasOtherWindowsInSession()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* /*context*/)
        {
            return !appContext()->sharedMemoryManager()->isLastInstanceInCurrentSession();
        });
}

ConditionWrapper hasOtherWindows()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* /*context*/)
        {
            return appContext()->sharedMemoryManager()->runningInstancesIndices().size() > 1;
        });
}

ConditionWrapper allowedToShowServersInResourceTree()
{
    return new CustomBoolCondition(
        [](const Parameters&, WindowContext* context)
        {
            return context->system()->accessController()->hasPowerUserPermissions()
                || context->system()->globalSettings()->showServersInTreeForNonAdmins();
        });
}

ConditionWrapper joystickConnected()
{
    return new CustomBoolCondition(
        [](const Parameters&, WindowContext* context)
        {
            return !appContext()->joystickManager()->devices().isEmpty();
        });
}

ConditionWrapper showBetaUpgradeWarning()
{
    return new CustomBoolCondition(
        [](const Parameters& /*params*/, WindowContext* /*context*/)
        {
            return !nx::branding::isDesktopClientCustomized()
                && nx::branding::isDevCloudHost();
        });
}

ConditionWrapper isCloudSystemConnectionUserInteractionRequired()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext*)
        {
            const auto systemId = parameters.argument(Qn::CloudSystemIdRole).toString();
            const auto cloudContext =
                appContext()->cloudCrossSystemManager()->systemContext(systemId);

            return cloudContext && cloudContext->status()
                == core::CloudCrossSystemContext::Status::connectionFailure;
        }
    );
}

ConditionWrapper videowallIsRunning()
{
    return new ResourceCondition(
        [](const QnResourcePtr& resource)
        {
            const auto videowall = resource.dynamicCast<QnVideoWallResource>();
            if (!videowall)
                return false;

            if (videowall->onlineItems().isEmpty())
                return false;

            return true;
        }, MatchMode::any);
}

ConditionWrapper canSaveLayoutAs()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* /*context*/)
        {
            auto resources = parameters.resources();
            if (resources.size() != 1)
                return false;

            auto layout = resources[0].dynamicCast<core::LayoutResource>();

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

ConditionWrapper isOwnLayout()
{
    return new CustomBoolCondition(
        [](const Parameters& parameters, WindowContext* context)
        {
            const auto user = context->system()->accessController()->user();
            if (!user)
                return false;

            const auto resources = parameters.resources();
            if (resources.size() != 1)
                return false;

            const auto layout = resources[0].objectCast<core::LayoutResource>();
            return layout && layout->getParentId() == user->getId();
        });
}

ConditionWrapper userHasCamerasWithEditableSettings()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            const auto& cameras = context->system()->resourcePool()->getAllCameras();
            const auto& accessController = context->system()->accessController();

            return std::any_of(cameras.begin(), cameras.end(),
                [accessController](auto camera)
                {
                    return accessController->hasPermissions(camera,
                        Qn::Permission::ReadWriteSavePermission);
                });
        }
    );
}

ConditionWrapper hasPermissionsForResources(Qn::Permissions permissions)
{
    return new ResourcesCondition(
        [permissions](const QnResourceList& resources, WindowContext* context)
        {
            return std::all_of(resources.begin(), resources.end(),
                [permissions](auto resource)
                {
                    return ResourceAccessManager::hasPermissions(resource, permissions);
                });
        });
}

ConditionWrapper hasWebPageSubtype(const std::optional<nx::vms::api::WebPageSubtype>& subtype)
{
    return new ResourceCondition(
        [=](const QnResourcePtr& resource)
        {
            const auto webPage = resource.dynamicCast<QnWebPageResource>();
            return webPage && (!subtype || webPage->subtype() == subtype);
        },
        MatchMode::all);
}

ConditionWrapper isIntegration()
{
    return hasWebPageSubtype(nx::vms::api::WebPageSubtype::clientApi);
}

ConditionWrapper isWebPage()
{
    return hasWebPageSubtype(nx::vms::api::WebPageSubtype::none);
}

ConditionWrapper isWebPageOrIntegration()
{
    return hasWebPageSubtype(std::nullopt);
}

ConditionWrapper homeTabIsNotActive(ActionVisibility defaultVisibility)
{
    return new CustomCondition(
        [defaultVisibility](const Parameters& /*parameters*/, WindowContext* context)
        {
            if (!ini().enableMultiSystemTabBar)
                return EnabledAction;

            const auto mainWindow = qobject_cast<MainWindow*>(context->mainWindowWidget());
            return mainWindow->titleBarStateStore()->state().homeTabActive
                ? defaultVisibility
                : EnabledAction;
        });
}

ConditionWrapper tierLimitsAllowMerge()
{
    return new CustomCondition(
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            const auto saas = context->system()->saasServiceManager();
            const auto tierLimit = saas->tierLimit(nx::vms::api::SaasTierLimitName::maxServers);
            if (tierLimit.has_value() && *tierLimit <= 1)
                return InvisibleAction;
            return EnabledAction;
        });
}

ConditionWrapper screenRecordingSupported()
{
    return new CustomCondition(
        [](const Parameters& /*params*/, WindowContext* context)
        {
            if (ini().enableDesktopCameraLazyInitialization)
                return nx::build_info::isWindows() ? EnabledAction : InvisibleAction;

            const auto screenRecordingAction = context->menu()->action(menu::ToggleScreenRecordingAction);
            if (screenRecordingAction)
            {
                const auto user = context->workbenchContext()->user();
                if (!user)
                    return InvisibleAction;

                const auto desktopCameraId = core::DesktopResource::calculateUniqueId(
                    context->system()->peerId(), user->getId());

                /* Do not check real pointer type to speed up check. */
                const auto desktopCamera =
                    context->system()->resourcePool()->getCameraByPhysicalId(desktopCameraId);
                if (desktopCamera && desktopCamera->hasFlags(Qn::desktop_camera))
                    return EnabledAction;

                return DisabledAction;
            }

            // Screen recording is not supported
            return InvisibleAction;
        });
}

ConditionWrapper customCellSpacingIsSet()
{
    return new CustomBoolCondition(
        [](const Parameters& /*parameters*/, WindowContext* context)
        {
            const auto layout = context->workbench()->currentLayout();
            const auto cellSpacing = layout->cellSpacing();

            return !core::LayoutResource::isEqualCellSpacing(core::CellSpacing::None, cellSpacing)
                && !core::LayoutResource::isEqualCellSpacing(core::CellSpacing::Small, cellSpacing)
                && !core::LayoutResource::isEqualCellSpacing(core::CellSpacing::Medium, cellSpacing)
                && !core::LayoutResource::isEqualCellSpacing(core::CellSpacing::Large, cellSpacing);
        });
}

ConditionWrapper hardwareVideoDecodingDisabled()
{
    return new CustomCondition(
        [](const Parameters& /*parameters*/, WindowContext* /*context*/)
        {
            return appContext()->localSettings()->hardwareDecodingEnabled()
                ? DisabledAction
                : EnabledAction;
        });
}

ConditionWrapper parentServerHasActiveBackupStorage()
{
    return new ResourcesCondition(
        [](const QnResourceList& resources, WindowContext* context)
        {
            return std::ranges::any_of(resources.filtered<QnVirtualCameraResource>(),
                [](const auto& camera)
                {
                    return camera->getParentServer()
                        && camera->getParentServer()->hasActiveBackupStorages();
                });
        });
}

} // namespace condition
} // namespace menu
} // namespace nx::vms::client::desktop
