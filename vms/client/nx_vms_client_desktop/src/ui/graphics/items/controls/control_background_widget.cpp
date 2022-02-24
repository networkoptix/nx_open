// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "control_background_widget.h"

#include <nx/vms/client/desktop/ui/common/color_theme.h>

using namespace nx::vms::client::desktop;

QnControlBackgroundWidget::QnControlBackgroundWidget(QGraphicsItem *parent):
    base_type(parent)
{
    setFrameColor(colorTheme()->color("dark8"));
    setFrameWidth(1.0);
    setWindowBrush(colorTheme()->color("dark3", 217));
}
