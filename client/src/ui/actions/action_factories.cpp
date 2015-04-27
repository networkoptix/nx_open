#include "action_factories.h"

#include <QtWidgets/QAction>

#include <utils/common/string.h>
#include <utils/resource_property_adaptors.h>
#include <utils/aspect_ratio.h>

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
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench.h>
#include <ui/style/globals.h>

QList<QAction *> QnOpenCurrentUserLayoutActionFactory::newActions(const QnActionParameters &, QObject *parent) {
    QnLayoutResourceList layouts = resourcePool()->getResourcesWithParentId(QnUuid()).filtered<QnLayoutResource>(); /* Multi-videos will go here. */
    if(context()->user())
        layouts.append(resourcePool()->getResourcesWithParentId(context()->user()->getId()).filtered<QnLayoutResource>());
    
    qSort(layouts.begin(), layouts.end(), [](const QnLayoutResourcePtr &l, const QnLayoutResourcePtr &r) {
        return naturalStringLess(l->getName(), r->getName());
    });

    QList<QAction *> result;
    foreach(const QnLayoutResourcePtr &layout, layouts) {
        if(snapshotManager()->isFile(layout))
            if(!QnWorkbenchLayout::instance(layout))
                continue; /* Not opened. */

        QAction *action = new QAction(parent);
        action->setText(layout->getName());
        action->setData(QVariant::fromValue<QnLayoutResourcePtr>(layout));
        connect(action, &QAction::triggered, this, &QnOpenCurrentUserLayoutActionFactory::at_action_triggered);

        result.push_back(action);
    }
    return result;
}

void QnOpenCurrentUserLayoutActionFactory::at_action_triggered() {
    QAction *action = dynamic_cast<QAction *>(sender());
    if(!action)
        return;

    QnLayoutResourcePtr layout = action->data().value<QnLayoutResourcePtr>();
    if(!layout)
        return;

    menu()->trigger(Qn::OpenSingleLayoutAction, layout);
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

    qSort(presets.begin(), presets.end(), [](const QnPtzPreset &l, const QnPtzPreset &r) {
        return naturalStringLess(l.name, r.name);
    });
    qSort(tours.begin(), tours.end(), [](const QnPtzTour &l, const QnPtzTour &r) {
        return naturalStringLess(l.name, r.name);
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
                .withArgument(Qn::ActionIdRole, static_cast<int>(Qn::PtzActivatePresetAction))
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
                .withArgument(Qn::ActionIdRole, static_cast<int>(Qn::PtzActivateTourAction))
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
    Qn::ActionId actionId = static_cast<Qn::ActionId>(parameters.argument<int>(Qn::ActionIdRole, Qn::NoAction));

    context()->menu()->trigger(actionId, parameters);
}


QMenu* QnEdgeNodeActionFactory::newMenu(const QnActionParameters &parameters, QWidget *parentWidget) {
    QnVirtualCameraResourcePtr edgeCamera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if (!edgeCamera || !QnMediaServerResource::isHiddenServer(edgeCamera->getParentResource()))
        return NULL;

    return menu()->newMenu(Qn::NoAction, Qn::TreeScope, parentWidget, QnActionParameters(edgeCamera->getParentResource()));
}


QList<QAction *> QnAspectRatioActionFactory::newActions(const QnActionParameters &parameters, QObject *parent) {
    QActionGroup *actionGroup = new QActionGroup(parent);

    QnWorkbenchLayout *currentLayout = workbench()->currentLayout();
    QnAspectRatio currentAspectRatio;

    if (currentLayout) {
        float current = currentLayout->cellAspectRatio();
        if (current <= 0)
            current = qnGlobals->defaultLayoutCellAspectRatio();
        currentAspectRatio = QnAspectRatio::closestStandardRatio(current);
    }

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
    if (!currentLayout)
        return;

    float aspectRatio = action->data().value<QnActionParameters>().argument(Qn::LayoutCellAspectRatioRole).toFloat();
    currentLayout->setCellAspectRatio(aspectRatio);
}
