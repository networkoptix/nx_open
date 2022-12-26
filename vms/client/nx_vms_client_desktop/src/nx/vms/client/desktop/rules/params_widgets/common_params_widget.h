// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "params_widget.h"

namespace nx::vms::client::desktop::rules {

/**
 * Used if no custom editor is provided. Dynamically creates pickers according to the passed
 * descriptor.
 */
class CommonParamsWidget: public ParamsWidget
{
    Q_OBJECT

public:
    CommonParamsWidget(SystemContext* context, QWidget* parent = nullptr);

private:
    virtual void onDescriptorSet() override;
};

} // namespace nx::vms::client::desktop::rules
