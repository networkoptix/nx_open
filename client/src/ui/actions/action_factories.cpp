#include "action_factories.h"

#include <QtWidgets/QAction>

#include <utils/common/string.h>

#include <core/kvpair/ptz_hotkey_kvpair_watcher.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_layout.h>

namespace {
    const char *ptzPresetIdPropertyName = "_qn_ptzPresetId";
}

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


QList<QAction *> QnPtzGoToPresetActionFactory::newActions(const QnActionParameters &parameters, QObject *parent) {

    //TODO: #GDM PTZ place actions in the same submenu as "Manage" and "Save preset"

    QList<QAction *> result;
    QnPtzPresetList presets;

    QnMediaResourceWidget* widget = parameters.mediaWidget();
    if (!widget || !widget->camera() || !widget->ptzController() || !widget->ptzController()->getPresets(&presets))
        return result;

    qSort(presets.begin(), presets.end(), [](const QnPtzPreset &l, const QnPtzPreset &r) {
        return naturalStringCaseInsensitiveLessThan(l.name, r.name);
    });

    QnPtzHotkeyKvPairWatcher* hotkeysWatcher = context()->instance<QnPtzHotkeyKvPairWatcher>();
    int resourceId = widget->camera()->getId();

    foreach(const QnPtzPreset &preset, presets) {
        QAction *action = new QAction(parent);
        action->setText(preset.name);

        int hotkey = hotkeysWatcher->hotkeyByPresetId(resourceId, preset.id);
        if(hotkey >= 0)
            action->setShortcut(Qt::Key_0 + hotkey);

        action->setData(QVariant::fromValue<QnMediaResourceWidget*>(widget));
        action->setProperty(ptzPresetIdPropertyName, preset.id);
        connect(action, SIGNAL(triggered()), this, SLOT(at_action_triggered()));

        result.push_back(action);
    }
    return result;
}

void QnPtzGoToPresetActionFactory::at_action_triggered() {
    QAction *action = dynamic_cast<QAction *>(sender());
    if(!action)
        return;

    QnMediaResourceWidget* widget = action->data().value<QnMediaResourceWidget*>();
    QString presetId = action->property(ptzPresetIdPropertyName).toString();
    if(!widget || presetId.isEmpty())
        return;

    context()->menu()->trigger(Qn::PtzGoToPresetAction, QnActionParameters(widget).withArgument(Qn::PtzPresetIdRole, presetId));
}
