#include "action_factories.h"

#include <QtGui/QAction>

#include <utils/common/string.h>

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_ptz_preset_manager.h>

namespace {
    struct LayoutNameCmp {
        bool operator()(const QnLayoutResourcePtr &l, const QnLayoutResourcePtr &r) {
            return naturalStringCaseInsensitiveLessThan(l->getName(), r->getName());
        }
    };

    struct PtzPresetNameCmp {
        bool operator()(const QnPtzPreset &l, const QnPtzPreset &r) {
            return naturalStringCaseInsensitiveLessThan(l.name, r.name);
        }
    };

} // anonymous namespace


QList<QAction *> QnOpenCurrentUserLayoutActionFactory::newActions(const QnActionParameters &, QObject *parent) {
    QnId userId = context()->user() ? context()->user()->getId() : QnId();
    QnLayoutResourceList layouts = resourcePool()->getResourcesWithParentId(userId).filtered<QnLayoutResource>();
    qSort(layouts.begin(), layouts.end(), LayoutNameCmp());

    QList<QAction *> result;
    foreach(const QnLayoutResourcePtr &layout, layouts) {
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
    QList<QAction *> result;

    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return result;

    QList<QnPtzPreset> presets = context()->instance<QnWorkbenchPtzPresetManager>()->ptzPresets(camera);
    qSort(presets.begin(), presets.end(), PtzPresetNameCmp());

    foreach(const QnPtzPreset &preset, presets) {
        QAction *action = new QAction(parent);
        action->setText(preset.name);
        if(preset.hotkey >= 0)
            action->setShortcut(Qt::Key_0 + preset.hotkey);
        action->setData(QVariant::fromValue<QnVirtualCameraResourcePtr>(camera));
        connect(action, SIGNAL(triggered()), this, SLOT(at_action_triggered()));

        result.push_back(action);
    }
    return result;
}

void QnPtzGoToPresetActionFactory::at_action_triggered() {
    QAction *action = dynamic_cast<QAction *>(sender());
    if(!action)
        return;

    QnVirtualCameraResourcePtr camera = action->data().value<QnVirtualCameraResourcePtr>();
    if(!camera)
        return;

    context()->menu()->trigger(Qn::PtzGoToPresetAction, QnActionParameters(camera).withArgument(Qn::ResourceNameRole, action->text()));
}
