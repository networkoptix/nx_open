// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_factories.h"

#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <QtQuick/QQuickItem>
#include <QtWidgets/QMenu>
#include <QtWidgets/QWidget>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/ptz/helpers.h>
#include <nx/vms/client/core/ptz/hotkey_resource_property_adaptor.h>
#include <nx/vms/client/desktop/common/widgets/webview_controller.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/radass/radass_resource_manager.h>
#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/web_resource_widget.h>
#include <ui/graphics/items/standard/graphics_web_view.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>

#include "action_manager.h"

namespace nx::vms::client::desktop {
namespace menu {

Factory::Factory(Manager* parent):
    QObject(parent),
    WindowContextAware(parent->windowContext())
{
}

Factory::ActionList Factory::newActions(const Parameters& /*parameters*/, QObject* /*parent*/)
{
    return ActionList();
}

QMenu* Factory::newMenu(const Parameters& /*parameters*/, QWidget* /*parentWidget*/)
{
    return nullptr;
}

OpenCurrentUserLayoutFactory::OpenCurrentUserLayoutFactory(Manager* parent):
    Factory(parent)
{
}

Factory::ActionList OpenCurrentUserLayoutFactory::newActions(const Parameters& /*parameters*/,
    QObject* parent)
{
    /* Multi-videos and shared layouts will go here. */
    auto layouts = system()->resourcePool()->getResourcesByParentId(QnUuid())
        .filtered<LayoutResource>();
    if (auto user = workbenchContext()->user())
    {
        layouts.append(system()->resourcePool()->getResourcesByParentId(user->getId())
            .filtered<LayoutResource>());
    }

    std::sort(layouts.begin(), layouts.end(),
        [](const QnLayoutResourcePtr& l, const QnLayoutResourcePtr& r)
        {
            return nx::utils::naturalStringLess(l->getName(), r->getName());
        });

    auto currentLayout = workbench()->currentLayout()->resource();

    ActionList result;
    for (const auto &layout: layouts)
    {
        if (layout->isFile())
        {
            if (!workbench()->layout(layout))
                continue; /* Not opened. */
        }

        if (layout->isServiceLayout())
            continue;

        // TODO: #sivanov Do not add preview search layouts to the resource pool.
        if (layout->isPreviewSearchLayout())
        {
            if (!workbench()->layout(layout))
                continue; /* Not opened. */
        }

        if (!ResourceAccessManager::hasPermissions(layout, Qn::ReadPermission))
            continue;

        auto action = new QAction(parent);
        action->setText(layout->getName());
        action->setCheckable(true);
        action->setChecked(layout == currentLayout);
        connect(action, &QAction::triggered, this,
            [this, layout]()
            {
                menu()->trigger(menu::OpenInNewTabAction, layout);
            });
        result.push_back(action);
    }
    return result;
}

PtzPresetsToursFactory::PtzPresetsToursFactory(Manager* parent):
    Factory(parent)
{
}

Factory::ActionList PtzPresetsToursFactory::newActions(const Parameters& parameters,
    QObject* parent)
{
    using namespace nx::vms::client::core::ptz;

    ActionList result;

    auto widget = parameters.widget<QnMediaResourceWidget>();
    if (!widget)
        return result;

    QnPtzPresetList presets;
    // In case of error empty list will be returned.
    helpers::getSortedPresets(widget->ptzController(), presets);

    QnPtzTourList tours;
    QnPtzObject activeObject;

    widget->ptzController()->getTours(&tours);
    widget->ptzController()->getActiveObject(&activeObject);

    std::sort(tours.begin(), tours.end(),
        [](const QnPtzTour& l, const QnPtzTour& r)
        {
            return nx::utils::naturalStringLess(l.name, r.name);
        });

    HotkeysResourcePropertyAdaptor adaptor;
    adaptor.setResource(widget->resource()->toResourcePtr());
    PresetIdByHotkey idByHotkey = adaptor.value();

    for (const auto& preset: presets)
    {
        auto action = new QAction(parent);
        if (activeObject.type == Qn::PresetPtzObject && activeObject.id == preset.id)
            action->setText(tr("%1 (active)", "Template for active PTZ preset").arg(preset.name));
        else
            action->setText(preset.name);

        int hotkey = idByHotkey.key(preset.id, kNoHotkey);
        if (hotkey != kNoHotkey)
            action->setShortcut(Qt::Key_0 + hotkey);

        connect(action, &QAction::triggered, this,
            [this, id = preset.id, parameters]
            {
                menu()->trigger(menu::PtzActivatePresetAction,
                    Parameters(parameters).withArgument(Qn::PtzObjectIdRole, id));
            });

        result.push_back(action);
    }

    if (!result.isEmpty())
    {
        auto separator = new QAction(parent);
        separator->setSeparator(true);
        result.push_back(separator);
    }

    for (const auto& tour: tours)
    {
        if (!tour.isValid(presets))
            continue;

        auto action = new QAction(parent);
        if (activeObject.type == Qn::TourPtzObject && activeObject.id == tour.id)
            action->setText(tr("%1 (active)", "Template for active PTZ tour").arg(tour.name));
        else
            action->setText(tour.name);

        int hotkey = idByHotkey.key(tour.id, kNoHotkey);
        if (hotkey != kNoHotkey)
            action->setShortcut(Qt::Key_0 + hotkey);

        connect(action, &QAction::triggered, this,
            [this, id = tour.id, parameters]
            {
                menu()->trigger(menu::PtzActivateTourAction,
                    Parameters(parameters).withArgument(Qn::PtzObjectIdRole, id));
            });

        result.push_back(action);
    }

    return result;
}

EdgeNodeFactory::EdgeNodeFactory(Manager* parent):
    Factory(parent)
{
}

QMenu* EdgeNodeFactory::newMenu(const Parameters& parameters, QWidget* parentWidget)
{
    const auto edgeCamera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if (!edgeCamera || !QnMediaServerResource::isHiddenEdgeServer(edgeCamera->getParentResource()))
        return nullptr;

    return menu()->newMenu(
        menu::NoAction,
        TreeScope,
        parentWidget,
        Parameters(edgeCamera->getParentResource()));
}

AspectRatioFactory::AspectRatioFactory(Manager* parent):
    Factory(parent)
{
}

Factory::ActionList AspectRatioFactory::newActions(const Parameters& /*parameters*/,
    QObject* parent)
{
    float current = workbench()->currentLayout()->cellAspectRatio();
    if (current <= 0)
        current = QnLayoutResource::kDefaultCellAspectRatio;
    QnAspectRatio currentAspectRatio = QnAspectRatio::closestStandardRatio(current);

    auto actionGroup = new QActionGroup(parent);
    for (const auto& aspectRatio: QnAspectRatio::standardRatios())
    {
        auto action = new QAction(parent);
        action->setText(aspectRatio.toString());
        action->setCheckable(true);
        action->setChecked(aspectRatio == currentAspectRatio);
        connect(action, &QAction::triggered, this,
            [this, ar = aspectRatio.toFloat()]
            {
                workbench()->currentLayoutResource()->setCellAspectRatio(ar);
            });
        actionGroup->addAction(action);
    }
    return actionGroup->actions();
}

ShowreelSettingsFactory::ShowreelSettingsFactory(Manager* parent):
    Factory(parent)
{
}

Factory::ActionList ShowreelSettingsFactory::newActions(
    const Parameters& parameters,
    QObject* parent)
{
    auto actionGroup = new QActionGroup(parent);
    actionGroup->setExclusive(true);

    auto id = parameters.argument<QnUuid>(Qn::UuidRole);
    const bool isCurrentShowreel = id.isNull();
    NX_ASSERT(!isCurrentShowreel || workbench()->currentLayout()->isShowreelReviewLayout());

    if (isCurrentShowreel)
        id = workbench()->currentLayout()->data(Qn::ShowreelUuidRole).value<QnUuid>();

    auto showreelManager = system()->showreelManager();
    const auto showreel = showreelManager->showreel(id);
    if (!NX_ASSERT(showreel.isValid()))
        return actionGroup->actions();

    const auto isManual = showreel.settings.manual;
    for (auto manual: {false, true})
    {
        auto action = new QAction(parent);
        action->setText(manual
            ? tr("Switch with Hotkeys")
            : tr("Switch on Timer"));
        action->setCheckable(true);
        action->setChecked(manual == isManual);
        connect(action, &QAction::triggered, this,
            [this, showreelManager, id, manual]
            {
                auto showreel = showreelManager->showreel(id);
                if (!NX_ASSERT(showreel.isValid()))
                    return;

                showreel.settings.manual = manual;
                showreelManager->addOrUpdateShowreel(showreel);
                menu()->trigger(menu::SaveShowreelAction, {Qn::UuidRole, id});
            });
        actionGroup->addAction(action);
    }
    return actionGroup->actions();
}

WebPageFactory::WebPageFactory(Manager* parent):
    Factory(parent)
{
}

Factory::ActionList WebPageFactory::newActions(const Parameters& parameters,
    QObject* parent)
{
    QPointer<QnWebResourceWidget> widget(qobject_cast<QnWebResourceWidget*>(parameters.widget()));
    if (!NX_ASSERT(widget))
        return {};

    return widget->webView()->controller()->getContextMenuActions(parent);
}

ShowOnItemsFactory::ShowOnItemsFactory(Manager* parent):
    Factory(parent)
{
}

Factory::ActionList ShowOnItemsFactory::newActions(const Parameters& parameters, QObject* parent)
{
    ActionList result;

    const QnResourceWidgetList widgets = parameters.widgets();

    if (const auto action = initInfoAction(parameters, parent))
        result.append(action);

    if (const auto action = initObjectsAction(parameters, parent))
        result.append(action);

    if (const auto action = initRoiAction(parameters, parent))
        result.append(action);

    if (const auto action = initHotspotsAction(parameters, parent))
        result.append(action);

    if (const auto action = initToolbarAction(parameters, parent))
        result.append(action);

    return result;
}

QAction* ShowOnItemsFactory::initInfoAction(const Parameters& parameters, QObject* parent)
{
    const QnResourceWidgetList widgets = parameters.widgets();
    QList<QnResourceWidget*> actualWidgets;
    for (const auto widget: widgets)
    {
        if (!(widget->visibleButtons() & Qn::InfoButton) && !widget->isInfoVisible())
            return nullptr;

        actualWidgets.append(widget);
    }

    const auto action = new QAction(tr("Info"), parent);
    action->setShortcuts({QKeySequence("I"), QKeySequence("Alt+I")});
    action->setShortcutVisibleInContextMenu(true);
    action->setCheckable(true);
    action->setChecked(std::all_of(actualWidgets.begin(), actualWidgets.end(),
        [](QnResourceWidget* widget) { return widget->isInfoVisible(); }));

    connect(action, &QAction::triggered, this,
        [this, actualWidgets, parameters](bool checked)
        {
            const bool animate = display()->animationAllowed();
            for (QnResourceWidget* widget: actualWidgets)
                widget->setInfoVisible(checked, animate);
        });

    return action;
}

QAction* ShowOnItemsFactory::initObjectsAction(const Parameters& parameters, QObject* parent)
{
    const QnResourceWidgetList widgets = parameters.widgets();
    QList<QnMediaResourceWidget*> actualWidgets;
    for (const auto widget: widgets)
    {
        const auto w = qobject_cast<QnMediaResourceWidget*>(widget);
        if (!w || !w->isAnalyticsSupported())
            return nullptr;
        actualWidgets.append(w);
    }

    const auto action = new QAction(tr("Objects"), parent);
    action->setCheckable(true);
    action->setChecked(std::all_of(actualWidgets.begin(), actualWidgets.end(),
        [](QnMediaResourceWidget* widget)
        {
            return widget->isAnalyticsObjectsVisible()
                || widget->isAnalyticsObjectsVisibleForcefully();
        }));
    action->setEnabled(std::none_of(actualWidgets.begin(), actualWidgets.end(),
        [](QnMediaResourceWidget* widget)
        {
            return widget->isAnalyticsObjectsVisibleForcefully();
        }));

    connect(action, &QAction::triggered, this,
        [this, actualWidgets, parameters](bool checked)
        {
            const bool animate = display()->animationAllowed();
            for (QnMediaResourceWidget* widget: actualWidgets)
                widget->setAnalyticsObjectsVisible(checked, animate);
        });

    return action;
}

QAction* ShowOnItemsFactory::initRoiAction(const Parameters& parameters, QObject* parent)
{
    const QnResourceWidgetList widgets = parameters.widgets();
    QList<QnMediaResourceWidget*> actualWidgets;
    for (const auto widget: widgets)
    {
        const auto w = qobject_cast<QnMediaResourceWidget*>(widget);
        if (!w)
            return nullptr;

        const auto camera = w->resource().dynamicCast<QnVirtualCameraResource>();
        if (!camera || camera->enabledAnalyticsEngines().isEmpty())
            return nullptr;
        actualWidgets.append(w);
    }

    const auto action = new QAction(tr("Regions of Interest"), parent);
    action->setCheckable(true);
    action->setChecked(std::all_of(actualWidgets.begin(), actualWidgets.end(),
        [](QnMediaResourceWidget* widget)
        {
            return widget->isRoiVisible();
        }));

    connect(action, &QAction::triggered, this,
        [this, actualWidgets, parameters](bool checked)
        {
            const bool animate = display()->animationAllowed();
            for (QnMediaResourceWidget* widget: actualWidgets)
                widget->setRoiVisible(checked, animate);
        });

    return action;
}

QAction* ShowOnItemsFactory::initHotspotsAction(const Parameters& parameters, QObject* parent)
{
    const QnResourceWidgetList widgets = parameters.widgets();

    QList<QnMediaResourceWidget*> widgetsWithHotspots;
    for (const auto& widget: widgets)
    {
        const auto mediaResourceWidget = dynamic_cast<QnMediaResourceWidget*>(widget);
        if (!mediaResourceWidget)
            continue;

        const auto camera = mediaResourceWidget->resource().dynamicCast<QnVirtualCameraResource>();
        if (!camera)
            continue;

        if (camera->cameraHotspotsEnabled())
            widgetsWithHotspots.append(mediaResourceWidget);
    }

    if (widgetsWithHotspots.empty())
        return nullptr;

    const auto action = new QAction(tr("Hotspots"), parent);
    action->setShortcuts({QKeySequence("H"), QKeySequence("Alt+H")});
    action->setShortcutVisibleInContextMenu(true);
    action->setCheckable(true);
    action->setChecked(std::all_of(widgetsWithHotspots.begin(), widgetsWithHotspots.end(),
        [](QnMediaResourceWidget* widget) { return widget->hotspotsVisible(); }));

    connect(action, &QAction::triggered, this,
        [widgetsWithHotspots, parameters](bool checked)
        {
            for (QnMediaResourceWidget* widget: widgetsWithHotspots)
                widget->setHotspotsVisible(checked);
        });

    return action;
}

QAction* ShowOnItemsFactory::initToolbarAction(const Parameters& parameters, QObject* parent)
{
    const QnResourceWidgetList widgets = parameters.widgets();

    QList<QnWebResourceWidget*> webResourceWidgets;
    for (const auto& widget: widgets)
    {
        const auto webResourceWidget = dynamic_cast<QnWebResourceWidget*>(widget);
        if (!webResourceWidget)
            continue;

        webResourceWidgets.append(webResourceWidget);
    }

    if (webResourceWidgets.empty())
        return nullptr;

    const auto action = new QAction(tr("Toolbar"), parent);
    action->setCheckable(true);

    const bool allToolbarsAreVisible = std::all_of(
        webResourceWidgets.begin(), webResourceWidgets.end(),
        [](QnWebResourceWidget* widget) { return widget->overlayIsVisible(); });
    action->setChecked(allToolbarsAreVisible);

    connect(action, &QAction::triggered, this,
        [webResourceWidgets, allToolbarsAreVisible](bool checked)
        {
            for (QnWebResourceWidget* widget: webResourceWidgets)
            {
                widget->setOverlayVisibility(!allToolbarsAreVisible);
            }
        });

    return action;
}

} // namespace menu
} // namespace nx::vms::client::desktop
