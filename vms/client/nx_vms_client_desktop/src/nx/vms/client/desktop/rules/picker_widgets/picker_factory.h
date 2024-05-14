// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/manifest.h>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

class PickerFactory
{
public:
    static PickerWidget* createWidget(
        vms::rules::Field* field,
        SystemContext* context,
        ParamsWidget* parent);
};

} // namespace nx::vms::client::desktop::rules
