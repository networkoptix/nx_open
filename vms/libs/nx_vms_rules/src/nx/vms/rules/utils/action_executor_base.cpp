// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_executor_base.h"

#include "../basic_action.h"

namespace nx::vms::rules {

ActionExecutorBase::ActionExecutorBase(QObject* parent):
    ActionExecutor(parent)
{
}

void ActionExecutorBase::execute(const ActionPtr& action)
{
    if (const auto handler = m_handlers.value(action->type()))
        handler(action);
}

} // namespace nx::vms::rules
