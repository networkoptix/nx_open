#include "action_factories.h"

#include <QtGui/QAction>

#include <utils/common/string_compare.h>

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>

namespace {
    struct LayoutNameCmp {
        bool operator()(const QnLayoutResourcePtr &l, const QnLayoutResourcePtr &r) {
            return qnNaturalStringLessThan(l->getName(), r->getName());
        }
    };

} // anonymous namespace


QList<QAction *> QnOpenCurrentUserLayoutActionFactory::newActions(QObject *parent) {
    QList<QAction *> result;

    QnId userId = context()->user() ? context()->user()->getId() : QnId();
    QnLayoutResourceList layouts = resourcePool()->getResourcesWithParentId(userId).filtered<QnLayoutResource>();
    qSort(layouts.begin(), layouts.end(), LayoutNameCmp());

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

