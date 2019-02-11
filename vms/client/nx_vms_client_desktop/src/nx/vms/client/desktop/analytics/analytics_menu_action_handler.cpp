#include "analytics_menu_action_handler.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtWidgets/QAction>

#include <api/global_settings.h>
#include <client/client_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include "analytics_objects_visualization_manager.h"

namespace {

static const QString kCacheDirectoryName = "analytics/objects_visualization";
static constexpr int kHintTimeoutMs = 5000;

QString getCacheDirectory()
{
    auto directory = QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    return directory.absoluteFilePath(kCacheDirectoryName);
}

} // namespace

namespace nx::vms::client::desktop {

struct AnalyticsMenuActionsHandler::Private
{
    AnalyticsObjectsVisualizationManager* manager = nullptr;
};

AnalyticsMenuActionsHandler::AnalyticsMenuActionsHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private)
{
    // Manager must be available from actions factory.
    d->manager = context()->instance<AnalyticsObjectsVisualizationManager>();
    d->manager->setCacheDirectory(getCacheDirectory());

    connect(action(ui::action::AnalyticsObjectsVisualizationModeAction), &QAction::triggered, this,
        &AnalyticsMenuActionsHandler::handleAnalyticsObjectsModeActionAction);

    // We must load settings as early as possible to handle auto-run showreels and layouts.
    connect(globalSettings(), &QnGlobalSettings::localSystemIdChangedDirect, this,
        &AnalyticsMenuActionsHandler::handleLocalSystemIdChanged);
}

AnalyticsMenuActionsHandler::~AnalyticsMenuActionsHandler()
{
}

void AnalyticsMenuActionsHandler::handleAnalyticsObjectsModeActionAction()
{
    const auto parameters = menu()->currentParameters(sender());

    const auto mode = static_cast<AnalyticsObjectsVisualizationMode>(
        parameters.argument(Qn::AnalyticsObjectsVisualizationModeRole).toInt());
    NX_ASSERT(mode != AnalyticsObjectsVisualizationMode::undefined);

    // If empty, means apply to the current layout.
    auto layoutItems = parameters.layoutItems();
    NX_ASSERT(!layoutItems.empty());

    d->manager->setMode(layoutItems, mode);

    if (!globalSettings()->localSystemId().isNull())
        d->manager->saveData(globalSettings()->localSystemId(), resourcePool());
}

void AnalyticsMenuActionsHandler::handleLocalSystemIdChanged()
{
    d->manager->switchLocalSystemId(globalSettings()->localSystemId());
}

} // namespace nx::vms::client::desktop
