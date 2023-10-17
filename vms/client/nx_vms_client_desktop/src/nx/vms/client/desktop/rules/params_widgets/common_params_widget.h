// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include "params_widget.h"

namespace nx::vms::client::desktop::rules {

class PickerWidget;

/**
 * Used if no custom editor is provided. Dynamically creates pickers according to the passed
 * descriptor.
 */
class CommonParamsWidget: public ParamsWidget
{
    Q_OBJECT

public:
    CommonParamsWidget(WindowContext* context, QWidget* parent = nullptr);

private:
    virtual void onDescriptorSet() override;
    virtual void updateUi() override;

    std::vector<PickerWidget*> m_pickers;
};

} // namespace nx::vms::client::desktop::rules
