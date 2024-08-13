// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "io_module_grid_overlay_contents.h"
#include "io_module_grid_overlay_contents_p.h"

#include <ui/common/palette.h>
#include <nx/vms/client/core/skin/color_theme.h>

using namespace nx::vms::client::core;

QnIoModuleGridOverlayContents::QnIoModuleGridOverlayContents() :
    base_type(),
    d_ptr(new QnIoModuleGridOverlayContentsPrivate(this))
{
    setPaletteColor(this, QPalette::WindowText, colorTheme()->color("dark13"));
    setPaletteColor(this, QPalette::HighlightedText, colorTheme()->color("green"));
    setPaletteColor(this, QPalette::ButtonText, colorTheme()->color("light4"));
    setPaletteColor(this, QPalette::Dark, colorTheme()->color("dark8"));
    setPaletteColor(this, QPalette::Button, colorTheme()->color("dark9"));
    setPaletteColor(this, QPalette::Midlight, colorTheme()->color("dark10"));
    setPaletteColor(this, QPalette::Shadow, colorTheme()->color("dark5", 77));

    setPaletteColor(this, QPalette::Disabled, QPalette::Button,
        colorTheme()->color("dark9", 77));
}

QnIoModuleGridOverlayContents::~QnIoModuleGridOverlayContents()
{
}

void QnIoModuleGridOverlayContents::portsChanged(const QnIOPortDataList& ports, bool userInputEnabled)
{
    Q_D(QnIoModuleGridOverlayContents);
    d->portsChanged(ports, userInputEnabled);
}

void QnIoModuleGridOverlayContents::stateChanged(const QnIOPortData& port, const QnIOStateData& state)
{
    Q_D(QnIoModuleGridOverlayContents);
    d->stateChanged(port, state);
}
