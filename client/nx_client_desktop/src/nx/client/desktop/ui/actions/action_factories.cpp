#include "action_factories.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QWidget>

#include <nx/utils/string.h>
#include <utils/resource_property_adaptors.h>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>

#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench.h>
#include <ui/style/globals.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

Factory::Factory(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
}

QList<QAction*> Factory::newActions(const Parameters& /*parameters*/, QObject* /*parent*/)
{
    return QList<QAction*>();
}

QMenu* Factory::newMenu(const Parameters& /*parameters*/, QWidget* /*parentWidget*/)
{
    return nullptr;
}

OpenCurrentUserLayoutFactory::OpenCurrentUserLayoutFactory(QObject* parent):
    Factory(parent)
{
}

QList<QAction*> OpenCurrentUserLayoutFactory::newActions(const Parameters& /*parameters*/,
    QObject* parent)
{
    /* Multi-videos and shared layouts will go here. */
    auto layouts = resourcePool()->getResourcesWithParentId(QnUuid()).filtered<QnLayoutResource>();
    if (context()->user())
    {
        layouts.append(resourcePool()->getResourcesWithParentId(context()->user()->getId())
            .filtered<QnLayoutResource>());
    }

    std::sort(layouts.begin(), layouts.end(),
        [](const QnLayoutResourcePtr& l, const QnLayoutResourcePtr& r)
        {
            return nx::utils::naturalStringLess(l->getName(), r->getName());
        });

    auto currentLayout = workbench()->currentLayout()->resource();

    QList<QAction*> result;
    for (const auto &layout: layouts)
    {
        if (layout->isFile())
        {
            if (!QnWorkbenchLayout::instance(layout))
                continue; /* Not opened. */
        }

        if (layout->isServiceLayout())
            continue;

        if (!accessController()->hasPermissions(layout, Qn::ReadPermission))
            continue;

        auto action = new QAction(parent);
        action->setText(layout->getName());
        action->setCheckable(true);
        action->setChecked(layout == currentLayout);
        connect(action, &QAction::triggered, this,
            [this, layout]()
            {
                menu()->trigger(action::OpenInNewTabAction, layout);
            });
        result.push_back(action);
    }
    return result;
}

PtzPresetsToursFactory::PtzPresetsToursFactory(QObject* parent):
    Factory(parent)
{
}

QList<QAction*> PtzPresetsToursFactory::newActions(const Parameters& parameters,
    QObject* parent)
{
    QList<QAction*> result;

    auto widget = parameters.widget<QnMediaResourceWidget>();
    if (!widget)
        return result;

    QnPtzPresetList presets;
    QnPtzTourList tours;
    QnPtzObject activeObject;
    widget->ptzController()->getPresets(&presets);
    widget->ptzController()->getTours(&tours);
    widget->ptzController()->getActiveObject(&activeObject);

    std::sort(presets.begin(), presets.end(),
        [](const QnPtzPreset& l, const QnPtzPreset& r)
        {
            return nx::utils::naturalStringLess(l.name, r.name);
        });

    std::sort(tours.begin(), tours.end(),
        [](const QnPtzTour& l, const QnPtzTour& r)
        {
            return nx::utils::naturalStringLess(l.name, r.name);
        });

    QnPtzHotkeysResourcePropertyAdaptor adaptor;
    adaptor.setResource(widget->resource()->toResourcePtr());
    QnPtzHotkeyHash idByHotkey = adaptor.value();

    for (const auto& preset: presets)
    {
        auto action = new QAction(parent);
        if (activeObject.type == Qn::PresetPtzObject && activeObject.id == preset.id)
            action->setText(tr("%1 (active)", "Template for active PTZ preset").arg(preset.name));
        else
            action->setText(preset.name);

        int hotkey = idByHotkey.key(preset.id, QnPtzHotkey::NoHotkey);
        if (hotkey != QnPtzHotkey::NoHotkey)
            action->setShortcut(Qt::Key_0 + hotkey);

        connect(action, &QAction::triggered, this,
            [this, id = preset.id, parameters]
            {
                menu()->trigger(action::PtzActivatePresetAction,
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

        int hotkey = idByHotkey.key(tour.id, QnPtzHotkey::NoHotkey);
        if (hotkey != QnPtzHotkey::NoHotkey)
            action->setShortcut(Qt::Key_0 + hotkey);

        connect(action, &QAction::triggered, this,
            [this, id = tour.id, parameters]
            {
                menu()->trigger(action::PtzActivateTourAction,
                    Parameters(parameters).withArgument(Qn::PtzObjectIdRole, id));
            });

        result.push_back(action);
    }

    return result;
}

EdgeNodeFactory::EdgeNodeFactory(QObject* parent):
    Factory(parent)
{
}

QMenu* EdgeNodeFactory::newMenu(const Parameters& parameters, QWidget* parentWidget)
{
    auto edgeCamera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if (!edgeCamera || !QnMediaServerResource::isHiddenServer(edgeCamera->getParentResource()))
        return nullptr;

    return menu()->newMenu(action::NoAction, TreeScope, parentWidget,
        Parameters(edgeCamera->getParentResource()));
}

AspectRatioFactory::AspectRatioFactory(QObject* parent):
    Factory(parent)
{
}

QList<QAction*> AspectRatioFactory::newActions(const Parameters& /*parameters*/,
    QObject* parent)
{
    float current = workbench()->currentLayout()->cellAspectRatio();
    if (current <= 0)
        current = qnGlobals->defaultLayoutCellAspectRatio();
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
                workbench()->currentLayout()->setCellAspectRatio(ar);
            });
        actionGroup->addAction(action);
    }
    return actionGroup->actions();
}

CurrentLayoutTourSettingsFactory::CurrentLayoutTourSettingsFactory(QObject* parent):
    Factory(parent)
{
}


QList<QAction*> CurrentLayoutTourSettingsFactory::newActions(const Parameters& /*parameters*/,
    QObject* parent)
{
    const auto isManual = workbench()->currentLayout()->data(Qn::LayoutTourIsManualRole).toBool();
    auto actionGroup = new QActionGroup(parent);
    actionGroup->setExclusive(true);
    for (auto manual: {false, true})
    {
        auto action = new QAction(parent);
        action->setText(manual
            ? tr("Switch with Hotkeys")
            : tr("Switch by Timer"));
        action->setCheckable(true);
        action->setChecked(manual == isManual);
        connect(action, &QAction::triggered, this,
            [this, manual]
            {
                workbench()->currentLayout()->setData(Qn::LayoutTourIsManualRole, manual);
                menu()->trigger(action::SaveCurrentLayoutTourAction);
            });
        actionGroup->addAction(action);
    }
    return actionGroup->actions();
}

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
