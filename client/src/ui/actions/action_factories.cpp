#include "action_factories.h"

#include <QtWidgets/QAction>

#include <utils/common/string.h>

#include <core/kvpair/ptz_hotkey_kvpair_adapter.h>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_layout.h>

QList<QAction *> QnOpenCurrentUserLayoutActionFactory::newActions(const QnActionParameters &, QObject *parent) {
    QnLayoutResourceList layouts = resourcePool()->getResourcesWithParentId(QnId()).filtered<QnLayoutResource>(); /* Multi-videos will go here. */
    if(context()->user())
        layouts.append(resourcePool()->getResourcesWithParentId(context()->user()->getId()).filtered<QnLayoutResource>());
    
    qSort(layouts.begin(), layouts.end(), [](const QnLayoutResourcePtr &l, const QnLayoutResourcePtr &r) {
        return naturalStringCaseInsensitiveLessThan(l->getName(), r->getName());
    });

    QList<QAction *> result;
    foreach(const QnLayoutResourcePtr &layout, layouts) {
        if(snapshotManager()->isFile(layout))
            if(!QnWorkbenchLayout::instance(layout))
                continue; /* Not opened. */

        QAction *action = new QAction(parent);
        action->setText(layout->getName());
        action->setData(QVariant::fromValue<QnLayoutResourcePtr>(layout));
        connect(action, SIGNAL(triggered()), this, SLOT(at_action_triggered()));

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
    widget->ptzController()->getPresets(&presets);
    widget->ptzController()->getTours(&tours);

    qSort(presets.begin(), presets.end(), [](const QnPtzPreset &l, const QnPtzPreset &r) {
        return naturalStringCaseInsensitiveLessThan(l.name, r.name);
    });
    qSort(tours.begin(), tours.end(), [](const QnPtzTour &l, const QnPtzTour &r) {
        return naturalStringCaseInsensitiveLessThan(l.name, r.name);
    });

    QnPtzHotkeyKvPairAdapter adapter(widget->resource()->toResourcePtr());

    foreach(const QnPtzPreset &preset, presets) {
        QAction *action = new QAction(parent);
        action->setText(preset.name);

        int hotkey = adapter.hotkeyByPresetId(preset.id);
        if(hotkey >= 0)
            action->setShortcut(Qt::Key_0 + hotkey);

        action->setData(QVariant::fromValue(
            QnActionParameters(parameters)
                .withArgument(Qn::PtzPresetIdRole, preset.id)
                .withArgument(Qn::ActionIdRole, static_cast<int>(Qn::PtzGoToPresetAction))
        ));
        connect(action, SIGNAL(triggered()), this, SLOT(at_action_triggered()));

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
        action->setText(tour.name);

        action->setData(QVariant::fromValue(
            QnActionParameters(parameters)
                .withArgument(Qn::PtzTourIdRole, tour.id)
                .withArgument(Qn::ActionIdRole, static_cast<int>(Qn::PtzStartTourAction))
        ));
        connect(action, SIGNAL(triggered()), this, SLOT(at_action_triggered()));

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
