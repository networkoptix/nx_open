// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "parameter_helper.h"

#include <client/client_globals.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>

namespace nx::vms::client::desktop {
namespace utils {

WidgetPtr extractParentWidget(
    const menu::Parameters& parameters,
    QWidget* defaultValue)
{
    return parameters.argument<WidgetPtr>(Qn::ParentWidgetRole, defaultValue);
}

} // namespace utils
} // namespace nx::vms::client::desktop
