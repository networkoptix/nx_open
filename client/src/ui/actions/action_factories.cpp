#include "action_factories.h"

#include <QtGui/QAction>

#include <utils/common/string_compare.h>

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
            return qnNaturalStringLessThan(l->getName(), r->getName());
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
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return QList<QAction *>();

    QList<QAction *> result;
    foreach(const QnPtzPreset &preset, context()->instance<QnWorkbenchPtzPresetManager>()->ptzPresets(camera)) {
        QAction *action = new QAction(parent);
        action->setText(preset.name);
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

    context()->menu()->trigger(Qn::PtzGoToPresetAction, QnActionParameters(camera).withArgument(Qn::NameParameter, action->text()));
}
