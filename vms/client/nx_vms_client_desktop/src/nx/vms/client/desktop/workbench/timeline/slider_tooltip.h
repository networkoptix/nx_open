// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/workbench/widgets/bubble_tooltip.h>

namespace nx::vms::client::desktop::workbench::timeline {

class SliderToolTip: public BubbleToolTip
{
    Q_OBJECT

public:
    explicit SliderToolTip(WindowContext* context, QObject* parent = nullptr);

protected:
    using BubbleToolTip::BubbleToolTip;
};

} // namespace nx::vms::client::desktop::workbench::timeline
