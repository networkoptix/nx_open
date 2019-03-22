#include "reconstruct_resolution.h"

#include <api/server_rest_connection.h>
#include <camera/camera_bookmarks_manager.h>
#include <common/common_module.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx_ec/ec_api.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_types.h>
#include <nx/vms/client/desktop/ui/actions/action_conditions.h>
#include <nx/vms/client/desktop/ui/actions/menu_factory.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/rules/action_parameters_processing.h>
#include <recording/time_period.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <utils/common/synctime.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop::integrations::entropix {

namespace {

static const QString kTag("entropix");
static const QString kEntropixServerUrl("http://localhost:8888/newnxevent");
static const QnUuid kVmsRuleId = QnUuid::fromArbitraryData(
    std::string("entropix::ReconstructResolutionIntegration"));
static const QString kVmsEventSource("ReconstructResolution");
static const QString kVmsEventCaption("Entropix: Reconstruct Resolution");

struct ZoomWindow
{
    qreal x1 = 0.0;
    qreal y1 = 0.0;
    qreal x2 = 0.0;
    qreal y2 = 0.0;

    ZoomWindow() = default;

    ZoomWindow(const QRectF& rect):
        x1(rect.left()),
        y1(rect.top()),
        x2(rect.right()),
        y2(rect.bottom())
    {
    }
};
#define ZoomWindow_Fields (x1)(y1)(x2)(y2)

struct Description
{
    QnUuid cameraId;
    QnUuid bookmarkId;
    std::chrono::milliseconds startTimeMs;
    std::chrono::milliseconds durationMs;
    std::vector<ZoomWindow> crops;
};
#define Description_Fields (cameraId)(bookmarkId)(startTimeMs)(durationMs)(crops)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((ZoomWindow)(Description), (json), _Fields)

class ZoomWindowIsSelected: public ui::action::Condition
{
public:
    ui::action::ActionVisibility check(
        const QnResourceWidgetList& widgets,
        QnWorkbenchContext* context)
    {
        return widgets.size() == 1 && widgets.first()->isZoomWindow()
            ? ui::action::EnabledAction
            : ui::action::InvisibleAction;
    }
};

bool cameraModelMatchesPattern(const QRegularExpression& pattern, const QnResourcePtr& resource)
{
    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
        return pattern.match(camera->getModel()).hasMatch();
    return false;
}

nx::vms::api::EventRuleData createVmsRule()
{
    nx::vms::api::EventRuleData rule;
    rule.id = kVmsRuleId;
    rule.eventType = nx::vms::api::EventType::userDefinedEvent;
    rule.actionType = nx::vms::api::ActionType::execHttpRequestAction;
    rule.aggregationPeriod = 0;
    rule.comment = "Entropix Reconstruct Resolution Rule";

    nx::vms::event::EventParameters eventParameters;
    eventParameters.resourceName = kVmsEventSource;
    rule.eventCondition = QJson::serialized(eventParameters);

    nx::vms::event::ActionParameters actionParameters;
    actionParameters.url = kEntropixServerUrl;
    actionParameters.requestType = nx::network::http::Method::post;
    actionParameters.text = nx::vms::rules::SubstitutionKeywords::Event::description;
    actionParameters.contentType = "application/json";
    rule.actionParams = QJson::serialized(actionParameters);

    return rule;
}

} // namespace

ReconstructResolutionIntegration::ReconstructResolutionIntegration(
    const QRegularExpression& models,
    QObject* parent)
    :
    base_type(parent),
    m_models(models)
{
}

void ReconstructResolutionIntegration::connectionEstablished(
    ec2::AbstractECConnectionPtr connection)
{
    if (!NX_ASSERT(connection))
        return;

    const auto manager = connection->getEventRulesManager(Qn::kSystemAccess);
    if (!NX_ASSERT(manager))
        return;

    const auto createRuleIfNotExistsHandler =
        [this, manager]
        (int /*handle*/, ec2::ErrorCode /*errorCode*/, const nx::vms::api::EventRuleDataList& rules)
        {
            const bool ruleExists = std::any_of(rules.cbegin(), rules.cend(),
                [](const auto& rule) { return rule.id == kVmsRuleId; });

            if (!ruleExists)
                manager->save(createVmsRule(), this, []{});
        };
    manager->getEventRules(this, createRuleIfNotExistsHandler);
}

void ReconstructResolutionIntegration::registerActions(ui::action::MenuFactory* factory)
{
    using namespace ui::action;

    const auto action = factory->registerAction()
        .flags(Slider | SingleTarget | WidgetTarget)
        .text(tr("Reconstruct Resolution"))
        .requiredGlobalPermission(GlobalPermission::manageBookmarks)
        .condition(
            !condition::isSafeMode()
            && ConditionWrapper(new AddBookmarkCondition())
            && ConditionWrapper(new ZoomWindowIsSelected())
            && ConditionWrapper(
                new ResourceCondition(
                    [this](const QnResourcePtr& resource)
                    {
                        return cameraModelMatchesPattern(m_models, resource);
                    },
                    MatchMode::All))
        )
        .action();

    connect(action, &QAction::triggered, this,
        [this, context = action->context()] { addReconstructResolutionBookmark(context); });
}

void ReconstructResolutionIntegration::addReconstructResolutionBookmark(
    QnWorkbenchContext* context)
{
    using std::chrono::milliseconds;

    const auto parameters = context->menu()->currentParameters(sender());
    const auto camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    const auto period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    const auto widgets = context->display()->widgets(camera);
    std::vector<QnResourceWidget*> selectedZoomWindows;
    std::copy_if(widgets.cbegin(), widgets.cend(),
        std::back_inserter(selectedZoomWindows),
        [](QnResourceWidget* widget) { return widget->isZoomWindow() && widget->isSelected(); });

    NX_ASSERT(!selectedZoomWindows.empty(), "Condition must not allow this");
    if (selectedZoomWindows.empty())
        return;

    QnCameraBookmark bookmark;
    bookmark.guid = QnUuid::createUuid();
    bookmark.name = tr("Reconstruct Resolution");
    bookmark.tags = {kTag};
    bookmark.startTimeMs = period.startTime();
    bookmark.durationMs = period.duration();
    bookmark.cameraId = camera->getId();
    bookmark.creatorId = context->user()->getId();
    bookmark.creationTimeStampMs = milliseconds(qnSyncTime->currentMSecsSinceEpoch());
    NX_ASSERT(bookmark.isValid());
    qnCameraBookmarksManager->addCameraBookmark(bookmark);
    context->action(ui::action::BookmarksModeAction)->setChecked(true);

    Description description;
    description.cameraId = bookmark.cameraId;
    description.bookmarkId = bookmark.guid;
    description.startTimeMs = bookmark.startTimeMs;
    description.durationMs = bookmark.durationMs;
    std::transform(selectedZoomWindows.cbegin(), selectedZoomWindows.cend(),
        std::back_inserter(description.crops),
        [](QnResourceWidget* widget) { return widget->zoomRect(); });

    nx::vms::event::EventMetaData metadata;
    metadata.cameraRefs.push_back(camera->getId().toString());

    context->commonModule()->currentServer()->restConnection()->createGenericEvent(
        /*source*/ kVmsEventSource,
        /*caption*/ kVmsEventCaption,
        /*description*/ QJson::serialized(description),
        /*metadata*/ metadata);
}

} // namespace nx::vms::client::desktop::integrations::entropix
