// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "font_config.h"

#include <nx/vms/client/desktop/application_context.h>

namespace nx::vms::client::desktop {

QFont FontConfig::small() const  { return font("small"); };
QFont FontConfig::normal() const { return font("normal"); };
QFont FontConfig::large() const { return font("large"); };
QFont FontConfig::xLarge() const { return font("xLarge"); };

FontConfig* fontConfig()
{
    return appContext()->fontConfig();
}

} // namespace nx::vms::client::desktop
