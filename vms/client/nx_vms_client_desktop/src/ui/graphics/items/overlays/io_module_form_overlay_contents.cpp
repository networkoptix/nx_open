// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "io_module_form_overlay_contents.h"
#include "io_module_form_overlay_contents_p.h"

#include <ui/common/palette.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

using namespace nx::vms::client::desktop;

QnIoModuleFormOverlayContents::QnIoModuleFormOverlayContents() :
    base_type(),
    d_ptr(new QnIoModuleFormOverlayContentsPrivate(this))
{
    setPaletteColor(this, QPalette::WindowText, colorTheme()->color("dark13"));
    setPaletteColor(this, QPalette::ButtonText, colorTheme()->color("light4"));
    setPaletteColor(this, QPalette::Dark, colorTheme()->color("dark10"));
    setPaletteColor(this, QPalette::Button, colorTheme()->color("dark11"));
    setPaletteColor(this, QPalette::Midlight, colorTheme()->color("dark12"));
    setPaletteColor(this, QPalette::Shadow, colorTheme()->color("dark5", 77));

    setPaletteColor(this, QPalette::Disabled, QPalette::Button,
        colorTheme()->color("dark11", 77));
}

QnIoModuleFormOverlayContents::~QnIoModuleFormOverlayContents()
{
}

void QnIoModuleFormOverlayContents::portsChanged(const QnIOPortDataList& ports, bool userInputEnabled)
{
    Q_D(QnIoModuleFormOverlayContents);
    d->portsChanged(ports, userInputEnabled);
}

void QnIoModuleFormOverlayContents::stateChanged(const QnIOPortData& port, const QnIOStateData& state)
{
    Q_D(QnIoModuleFormOverlayContents);
    d->stateChanged(port, state);
}
