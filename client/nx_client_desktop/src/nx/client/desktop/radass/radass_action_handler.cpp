#include "radass_action_handler.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>

#include <QtWidgets/QAction>

#include <api/global_settings.h>

#include <client/client_module.h>

#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>

#include <camera/resource_display.h>

#include <nx/client/desktop/radass/radass_types.h>
#include <nx/client/desktop/radass/radass_controller.h>
#include <nx/client/desktop/radass/radass_resource_manager.h>
#include <nx/client/desktop/radass/radass_support.h>
#include <nx/client/desktop/radass/radass_cameras_watcher.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/graphics/items/generic/graphics_message_box.h>

namespace {

static const QString kCacheDirectoryName = lit("radass");
static constexpr int kHintTimeoutMs = 5000;

QString getCacheDirectory()
{
    auto directory = QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    return directory.absoluteFilePath(kCacheDirectoryName);
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

struct RadassActionHandler::Private
{
    QScopedPointer<RadassCamerasWatcher> camerasWatcher;
    RadassController* controller = nullptr;
    RadassResourceManager* manager = nullptr;

    bool notifiedAboutPerformanceLoss = false;
    QPointer<QnGraphicsMessageBox> performanceHintLabel;
};

RadassActionHandler::RadassActionHandler(QObject* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private)
{
    d->controller = qnClientModule->radassController();
    d->camerasWatcher.reset(new RadassCamerasWatcher(d->controller, resourcePool()));
    // Manager must be available from actions factory.
    d->manager = context()->instance<RadassResourceManager>();
    d->manager->setCacheDirectory(getCacheDirectory());

    connect(action(ui::action::RadassAction), &QAction::triggered, this,
        &RadassActionHandler::at_radassAction_triggered);

    connect(d->manager, &RadassResourceManager::modeChanged, this,
        &RadassActionHandler::handleItemModeChanged);

    connect(d->controller, &RadassController::performanceCanBeImproved, this,
        &RadassActionHandler::notifyAboutPerformanceLoss);

    connect(workbench(), &QnWorkbench::currentLayoutChanged, this,
        &RadassActionHandler::handleCurrentLayoutChanged);

    // We must load settings as early as possible to handle auto-run showreels and layouts.
    connect(globalSettings(), &QnGlobalSettings::localSystemIdChangedDirect, this,
        &RadassActionHandler::handleLocalSystemIdChanged);
}

RadassActionHandler::~RadassActionHandler()
{

}

void RadassActionHandler::at_radassAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    const auto mode = static_cast<RadassMode>(parameters.argument(Qn::ResolutionModeRole).toInt());
    if (mode == RadassMode::Custom)
        return;

    // If empty, means apply to the current layout.
    auto layoutItems = parameters.layoutItems();
    if (layoutItems.empty())
        d->manager->setMode(workbench()->currentLayout()->resource(), mode);
    else
        d->manager->setMode(layoutItems, mode);

    if (!globalSettings()->localSystemId().isNull())
        d->manager->saveData(globalSettings()->localSystemId(), resourcePool());
}

void RadassActionHandler::handleItemModeChanged(const QnLayoutItemIndex& item, RadassMode mode)
{
    NX_ASSERT(item.layout());
    if (!item.layout())
        return;

    // Ignore items that do not belong to the current layout.
    if (item.layout() != workbench()->currentLayout()->resource())
        return;

    NX_ASSERT(isRadassSupported(item));

    auto widget = display()->widget(item.uuid());
    if (auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget))
    {
        auto camDisplay = mediaWidget->display()->camDisplay();
        d->controller->setMode(camDisplay, mode);
    }
}

void RadassActionHandler::handleCurrentLayoutChanged()
{
    const auto layout = workbench()->currentLayout()->resource();
    if (!layout)
        return;

    for (const auto& item: workbench()->currentLayout()->items())
    {
        QnLayoutItemIndex index(layout, item->uuid());
        if (!isRadassSupported(index))
            continue;

        auto widget = display()->widget(index.uuid());
        if (auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget))
        {
            auto camDisplay = mediaWidget->display()->camDisplay();
            d->controller->setMode(camDisplay, d->manager->mode(index));
        }
    }

    // Notify once for each layout.
    d->notifiedAboutPerformanceLoss = false;
    if (d->performanceHintLabel)
        d->performanceHintLabel->hideImmideately();
    d->performanceHintLabel.clear();
}

void RadassActionHandler::handleLocalSystemIdChanged()
{
    d->manager->switchLocalSystemId(globalSettings()->localSystemId());
    handleCurrentLayoutChanged();
}

void RadassActionHandler::notifyAboutPerformanceLoss()
{
    if (d->notifiedAboutPerformanceLoss)
        return;

    const auto hint = tr("Set layout resolution to \"Auto\" to increase performance.");
    d->performanceHintLabel = QnGraphicsMessageBox::information(hint, kHintTimeoutMs);
    d->notifiedAboutPerformanceLoss = true;
}

} // namespace desktop
} // namespace client
} // namespace nx

