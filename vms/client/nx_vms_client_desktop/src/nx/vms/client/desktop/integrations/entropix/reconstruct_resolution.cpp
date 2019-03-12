#include "reconstruct_resolution.h"

#include <camera/camera_bookmarks_manager.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>

#include <recording/time_period.h>

#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_types.h>
#include <nx/vms/client/desktop/ui/actions/action_conditions.h>
#include <nx/vms/client/desktop/ui/actions/menu_factory.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>

#include <utils/common/synctime.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop::integrations::entropix {

namespace {

static const QString kTag("entropix");

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
    std::vector<ZoomWindow> crops;
};
#define Description_Fields (crops)

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

} // namespace

ReconstructResolutionIntegration::ReconstructResolutionIntegration(
    const QRegularExpression& models,
    QObject* parent)
    :
    base_type(parent),
    m_models(models)
{
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

    Description description;
    std::transform(selectedZoomWindows.cbegin(), selectedZoomWindows.cend(),
        std::back_inserter(description.crops),
        [](QnResourceWidget* widget) { return widget->zoomRect(); });

    QnCameraBookmark bookmark;
    bookmark.guid = QnUuid::createUuid();
    bookmark.name = tr("Reconstruct Resolution");
    bookmark.description = QJson::serialized(description);
    bookmark.tags = {kTag};
    bookmark.startTimeMs = period.startTime();
    bookmark.durationMs = period.duration();
    bookmark.cameraId = camera->getId();
    bookmark.creatorId = context->user()->getId();
    bookmark.creationTimeStampMs = milliseconds(qnSyncTime->currentMSecsSinceEpoch());
    NX_ASSERT(bookmark.isValid());

    qnCameraBookmarksManager->addCameraBookmark(bookmark);

    context->action(ui::action::BookmarksModeAction)->setChecked(true);
}

} // namespace nx::vms::client::desktop::integrations::entropix
