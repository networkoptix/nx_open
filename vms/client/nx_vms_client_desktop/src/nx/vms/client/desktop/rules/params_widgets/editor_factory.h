// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/manifest.h>

#include "params_widget.h"

class WindowContext;

namespace nx::vms::client::desktop::rules {

class EventEditorFactory
{
public:
    static ParamsWidget* createWidget(
        const vms::rules::ItemDescriptor& descriptor,
        WindowContext* context,
        QWidget* parent = nullptr);
};

class ActionEditorFactory
{
public:
    static ParamsWidget* createWidget(
        const vms::rules::ItemDescriptor& descriptor,
        WindowContext* context,
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop::rules
