#include "action_factories.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QWidget>

#include <nx/utils/string.h>
#include <utils/resource_property_adaptors.h>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench.h>
#include <ui/style/globals.h>

QnOpenCurrentUserLayoutActionFactory::QnOpenCurrentUserLayoutActionFactory(QObject *parent):
    QnActionFactory(parent)
{
}

QList<QAction *> QnOpenCurrentUserLayoutActionFactory::newActions(const QnActionParameters &, QObject *parent)
{
    /* Multi-videos and shared layouts will go here. */
    auto layouts = resourcePool()->getResourcesWithParentId(QnUuid()).filtered<QnLayoutResource>();
    if (context()->user())
        layouts.append(resourcePool()->getResourcesWithParentId(context()->user()->getId())
            .filtered<QnLayoutResource>());

    std::sort(layouts.begin(), layouts.end(),
        [](const QnLayoutResourcePtr &l, const QnLayoutResourcePtr &r)
        {
            return nx::utils::naturalStringLess(l->getName(), r->getName());
        });

    auto currentLayout = context()->workbench()->currentLayout()->resource();

    QList<QAction *> result;
    for (const auto &layout : layouts)
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

        QAction *action = new QAction(parent);
        action->setText(layout->getName());
        action->setData(QVariant::fromValue<QnLayoutResourcePtr>(layout));
        action->setCheckable(true);
        action->setChecked(layout == currentLayout);
        connect(action, &QAction::triggered, this,
            [this, layout]()
            {
                menu()->trigger(QnActions::OpenSingleLayoutAction, layout);
            });
        result.push_back(action);
    }
    return result;
}

QList<QAction *> QnPtzPresetsToursActionFactory::newActions(const QnActionParameters &parameters, QObject *parent) {
    QList<QAction *> result;

    QnMediaResourceWidget* widget = parameters.widget<QnMediaResourceWidget>();
    if (!widget)
        return result;

    QnPtzPresetList presets;
    QnPtzTourList tours;
    QnPtzObject activeObject;
    widget->ptzController()->getPresets(&presets);
    widget->ptzController()->getTours(&tours);
    widget->ptzController()->getActiveObject(&activeObject);

    std::sort(presets.begin(), presets.end(), [](const QnPtzPreset &l, const QnPtzPreset &r) {
        return nx::utils::naturalStringLess(l.name, r.name);
    });
    std::sort(tours.begin(), tours.end(), [](const QnPtzTour &l, const QnPtzTour &r) {
        return nx::utils::naturalStringLess(l.name, r.name);
    });

    QnPtzHotkeysResourcePropertyAdaptor adaptor;
    adaptor.setResource(widget->resource()->toResourcePtr());
    QnPtzHotkeyHash idByHotkey = adaptor.value();

    foreach(const QnPtzPreset &preset, presets) {
        QAction *action = new QAction(parent);
        if(activeObject.type == Qn::PresetPtzObject && activeObject.id == preset.id) {
            action->setText(tr("%1 (active)", "Template for active PTZ preset").arg(preset.name));
        } else {
            action->setText(preset.name);
        }

        int hotkey = idByHotkey.key(preset.id, QnPtzHotkey::NoHotkey);
        if(hotkey != QnPtzHotkey::NoHotkey)
            action->setShortcut(Qt::Key_0 + hotkey);

        action->setData(QVariant::fromValue(
            QnActionParameters(parameters)
                .withArgument(Qn::PtzObjectIdRole, preset.id)
                .withArgument(Qn::ActionIdRole, static_cast<int>(QnActions::PtzActivatePresetAction))
        ));
        connect(action, &QAction::triggered, this, &QnPtzPresetsToursActionFactory::at_action_triggered);

        result.push_back(action);
    }

    if(!result.isEmpty()) {
        QAction *separator = new QAction(parent);
        separator->setSeparator(true);
        result.push_back(separator);
    }

    foreach(const QnPtzTour &tour, tours) {
        if (!tour.isValid(presets))
            continue;

        QAction *action = new QAction(parent);
        if(activeObject.type == Qn::TourPtzObject && activeObject.id == tour.id) {
            action->setText(tr("%1 (active)", "Template for active PTZ tour").arg(tour.name));
        } else {
            action->setText(tour.name);
        }

        int hotkey = idByHotkey.key(tour.id, QnPtzHotkey::NoHotkey);
        if(hotkey != QnPtzHotkey::NoHotkey)
            action->setShortcut(Qt::Key_0 + hotkey);

        action->setData(QVariant::fromValue(
            QnActionParameters(parameters)
                .withArgument(Qn::PtzObjectIdRole, tour.id)
                .withArgument(Qn::ActionIdRole, static_cast<int>(QnActions::PtzActivateTourAction))
        ));
        connect(action, &QAction::triggered, this, &QnPtzPresetsToursActionFactory::at_action_triggered);

        result.push_back(action);
    }

    return result;
}

void QnPtzPresetsToursActionFactory::at_action_triggered() {
    QAction *action = dynamic_cast<QAction *>(sender());
    if(!action)
        return;

    QnActionParameters parameters = action->data().value<QnActionParameters>();
    QnActions::IDType actionId = static_cast<QnActions::IDType>(
        parameters.argument<int>(Qn::ActionIdRole, QnActions::NoAction));

    context()->menu()->trigger(actionId, parameters);
}


QMenu* QnEdgeNodeActionFactory::newMenu(const QnActionParameters &parameters, QWidget *parentWidget) {
    QnVirtualCameraResourcePtr edgeCamera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if (!edgeCamera || !QnMediaServerResource::isHiddenServer(edgeCamera->getParentResource()))
        return NULL;

    using namespace nx::client::desktop::ui::action;
    return menu()->newMenu(QnActions::NoAction, TreeScope, parentWidget,
        QnActionParameters(edgeCamera->getParentResource()));
}

QList<QAction *> QnAspectRatioActionFactory::newActions(const QnActionParameters &parameters, QObject *parent) {
    QActionGroup *actionGroup = new QActionGroup(parent);

    QnWorkbenchLayout *currentLayout = workbench()->currentLayout();

    float current = currentLayout->cellAspectRatio();
    if (current <= 0)
        current = qnGlobals->defaultLayoutCellAspectRatio();
    QnAspectRatio currentAspectRatio = QnAspectRatio::closestStandardRatio(current);


    for (const QnAspectRatio &aspectRatio: QnAspectRatio::standardRatios()) {
        QAction *action = new QAction(parent);
        action->setText(aspectRatio.toString());
        action->setData(QVariant::fromValue(
            QnActionParameters(parameters).withArgument(Qn::LayoutCellAspectRatioRole, aspectRatio.toFloat())
        ));
        action->setCheckable(true);
        action->setChecked(aspectRatio == currentAspectRatio);
        actionGroup->addAction(action);
    }

    connect(actionGroup, &QActionGroup::triggered, this, &QnAspectRatioActionFactory::at_action_triggered);

    return actionGroup->actions();
}

void QnAspectRatioActionFactory::at_action_triggered(QAction *action) {
    QnWorkbenchLayout *currentLayout = workbench()->currentLayout();

    float aspectRatio = action->data().value<QnActionParameters>().argument(Qn::LayoutCellAspectRatioRole).toFloat();
    currentLayout->setCellAspectRatio(aspectRatio);
}
