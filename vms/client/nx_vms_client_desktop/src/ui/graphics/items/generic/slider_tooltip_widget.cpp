// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "slider_tooltip_widget.h"

#include <QtWidgets/QApplication>

#include <ui/common/palette.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

using namespace nx::vms::client::desktop;

QnSliderTooltipWidget::QnSliderTooltipWidget(QGraphicsItem* parent):
    base_type(parent)
{
    setPaletteColor(this, QPalette::Window, colorTheme()->color("light4"));
    setPaletteColor(this, QPalette::WindowText, colorTheme()->color("dark5"));
    setPaletteColor(this, QPalette::Highlight, Qt::transparent);

    setRoundingRadius(0.0);
    setTailLength(3.0);
    setTailWidth(6.0);
    setFont(QApplication::font());
}
