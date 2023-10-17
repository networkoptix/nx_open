// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "slider_tooltip.h"

namespace nx::vms::client::desktop::workbench::timeline {

SliderToolTip::SliderToolTip(WindowContext* context, QObject* parent):
    BubbleToolTip(context, QUrl("Nx/Timeline/private/SliderToolTip.qml"), parent)
{
    setOrientation(Qt::Vertical);
}

} // namespace nx::vms::client::desktop::workbench::timeline
