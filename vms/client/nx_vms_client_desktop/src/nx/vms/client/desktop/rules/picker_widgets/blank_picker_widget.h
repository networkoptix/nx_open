// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

/** Does nothing. Used for unknown type of fields. */
class BlankPicker: public PickerWidget
{
    Q_OBJECT

public:
    BlankPicker(QnWorkbenchContext* context, CommonParamsWidget* parent);

private:
    virtual void onDescriptorSet() override;
    virtual void updateUi() override;
};

} // namespace nx::vms::client::desktop::rules
