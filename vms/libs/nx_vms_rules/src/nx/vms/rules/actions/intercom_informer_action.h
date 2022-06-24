// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/basic_action.h>

namespace nx::vms::rules {

class NX_VMS_RULES_API IntercomInformerAction: public nx::vms::rules::BasicAction
{
    Q_OBJECT
    Q_CLASSINFO("type", "nx.actions.intercomInformer")
};

} // namespace nx::vms::rules
