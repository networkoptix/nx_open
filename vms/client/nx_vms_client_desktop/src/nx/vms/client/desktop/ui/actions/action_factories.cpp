#include "action_factories.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QWidget>
#include <QtWebEngineWidgets/QWebEnginePage>
#include <QtQuick/QQuickItem>

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

#include <nx/vms/client/desktop/radass/radass_types.h>
#include <nx/vms/client/desktop/radass/radass_resource_manager.h>
#include <nx/vms/client/desktop/ini.h>

#include <nx/client/ptz/ptz_helpers.h>
#include <nx/client/ptz/ptz_hotkey_resource_property_adaptor.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/resource/web_resource_widget.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench.h>
#include <ui/style/globals.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace action {

Factory::Factory(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
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

OpenCurrentUserLayoutFactory::OpenCurrentUserLayoutFactory(QObject* parent):
    Factory(parent)
{
}

Factory::ActionList OpenCurrentUserLayoutFactory::newActions(const Parameters& /*parameters*/,
    QObject* parent)
{
    /* Multi-videos and shared layouts will go here. */
    auto layouts = resourcePool()->getResourcesByParentId(QnUuid()).filtered<QnLayoutResource>();
    if (context()->user())
    {
        layouts.append(resourcePool()->getResourcesByParentId(context()->user()->getId())
            .filtered<QnLayoutResource>());
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
            if (!QnWorkbenchLayout::instance(layout))
                continue; /* Not opened. */
        }

        if (layout->isServiceLayout())
            continue;

        // TODO: #GDM do not add preview search layouts to the resource pool
        if (layout->data().contains(Qn::LayoutSearchStateRole))
        {
            if (!QnWorkbenchLayout::instance(layout))
                continue; /* Not opened. */
        }

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

Factory::ActionList PtzPresetsToursFactory::newActions(const Parameters& parameters,
    QObject* parent)
{
    ActionList result;

    auto widget = parameters.widget<QnMediaResourceWidget>();
    if (!widget)
        return result;

    QnPtzPresetList presets;
    // In case of error empty list will be returned.
    nx::vms::client::core::ptz::helpers::getSortedPresets(widget->ptzController(), presets);

    QnPtzTourList tours;
    QnPtzObject activeObject;

    widget->ptzController()->getTours(&tours);
    widget->ptzController()->getActiveObject(&activeObject);

    std::sort(tours.begin(), tours.end(),
        [](const QnPtzTour& l, const QnPtzTour& r)
        {
            return nx::utils::naturalStringLess(l.name, r.name);
        });

    nx::vms::client::core::ptz::PtzHotkeysResourcePropertyAdaptor adaptor;
    adaptor.setResource(widget->resource()->toResourcePtr());
    QnPtzIdByHotkeyHash idByHotkey = adaptor.value();

    for (const auto& preset: presets)
    {
        auto action = new QAction(parent);
        if (activeObject.type == Qn::PresetPtzObject && activeObject.id == preset.id)
            action->setText(tr("%1 (active)", "Template for active PTZ preset").arg(preset.name));
        else
            action->setText(preset.name);

        int hotkey = idByHotkey.key(preset.id, QnPtzHotkey::kNoHotkey);
        if (hotkey != QnPtzHotkey::kNoHotkey)
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

        int hotkey = idByHotkey.key(tour.id, QnPtzHotkey::kNoHotkey);
        if (hotkey != QnPtzHotkey::kNoHotkey)
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

Factory::ActionList AspectRatioFactory::newActions(const Parameters& /*parameters*/,
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

LayoutTourSettingsFactory::LayoutTourSettingsFactory(QObject* parent):
    Factory(parent)
{
}


Factory::ActionList LayoutTourSettingsFactory::newActions(const Parameters& parameters,
    QObject* parent)
{
    auto actionGroup = new QActionGroup(parent);
    actionGroup->setExclusive(true);

    auto id = parameters.argument<QnUuid>(Qn::UuidRole);
    const bool isCurrentTour = id.isNull();
    NX_ASSERT(!isCurrentTour || workbench()->currentLayout()->isLayoutTourReview());

    if (isCurrentTour)
        id = workbench()->currentLayout()->data(Qn::LayoutTourUuidRole).value<QnUuid>();

    const auto tour = layoutTourManager()->tour(id);
    NX_ASSERT(tour.isValid());
    if (!tour.isValid())
        return actionGroup->actions();

    const auto isManual = tour.settings.manual;
    for (auto manual: {false, true})
    {
        auto action = new QAction(parent);
        action->setText(manual
            ? tr("Switch with Hotkeys")
            : tr("Switch on Timer"));
        action->setCheckable(true);
        action->setChecked(manual == isManual);
        connect(action, &QAction::triggered, this,
            [this, id, manual]
            {
                auto tour = layoutTourManager()->tour(id);
                NX_ASSERT(tour.isValid());
                if (!tour.isValid())
                    return;

                tour.settings.manual = manual;
                layoutTourManager()->addOrUpdateTour(tour);
                menu()->trigger(action::SaveLayoutTourAction, {Qn::UuidRole, id});
            });
        actionGroup->addAction(action);
    }
    return actionGroup->actions();
}


WebPageFactory::WebPageFactory(QObject* parent):
    Factory(parent)
{
}

Factory::ActionList WebPageFactory::newActions(const Parameters& parameters,
    QObject* parent)
{
    ActionList result;

    QPointer<QnWebResourceWidget> widget(qobject_cast<QnWebResourceWidget*>(parameters.widget()));
    if (!NX_ASSERT(widget))
        return result;

    const auto allowedActions = {
        QWebEnginePage::Back,
        QWebEnginePage::Forward,
        QWebEnginePage::Stop,
        QWebEnginePage::Reload,
        QWebEnginePage::SavePage
    };

    for (auto actionId: allowedActions)
    {
        if (!widget->isWebActionEnabled(actionId))
            continue;
        auto action = new QAction(parent);
        action->setText(widget->webActionText(actionId));
        connect(action, &QAction::triggered, this,
            [actionId, widget]
            {
                if (!widget)
                    return;

                widget->triggerWebAction(actionId);
            });
        result.push_back(action);
    }

    return result;
}

} // namespace action
} // namespace ui
} // namespace nx::vms::client::desktop
