// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "panel.h"

#include <ui/common/palette.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

namespace nx::vms::client::desktop {

Panel::Panel(QWidget* parent): QWidget(parent)
{
    setPaletteColor(this, QPalette::Window, colorTheme()->color("dark8"));
    setAutoFillBackground(true);
}

} // namespace nx::vms::client::desktop
