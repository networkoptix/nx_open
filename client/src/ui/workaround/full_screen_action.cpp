#include "full_screen_action.h"

#include <cassert>

#include <utils/common/variant.h>

#include <ui/workbench/workbench_context.h>


namespace {
    const char *const qn_fullScreenActionIdName = "_qn_fullScreenActionId";
}

bool QnFullScreenAction::isAtFglrx(QnWorkbenchContext *context) {
    return true; // TODO: implement me
}

QAction *QnFullScreenAction::get(QnWorkbenchContext *context) {
    assert(context);

    Qn::ActionId id = static_cast<Qn::ActionId>(qvariant_cast<int>(context->property(qn_fullScreenActionIdName), Qn::NoAction));
    if(id == Qn::NoAction) {
        id = isAtFglrx(context) ? Qn::MaximizeAction : Qn::FullscreenAction;
        context->setProperty(qn_fullScreenActionIdName, static_cast<int>(id));
    }

    return context->action(id);
}

