#pragma once

#include "basic_action.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API ActionExecutor
{
public:
    virtual void execute(const ActionPtr& action) = 0;
};

} // namespace nx::vms::rules