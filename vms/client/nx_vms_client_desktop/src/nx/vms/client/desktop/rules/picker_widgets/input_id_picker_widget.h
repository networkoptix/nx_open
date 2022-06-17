// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "picker_widget.h"

namespace Ui { class InputIDPickerWidget; }

namespace nx::vms::client::desktop::rules {

class InputIDPickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    InputIDPickerWidget(common::SystemContext* context, QWidget* parent = nullptr);
    virtual ~InputIDPickerWidget() override;

    virtual void setReadOnly(bool value) override;

private:
    QScopedPointer<Ui::InputIDPickerWidget> ui;

    virtual void onDescriptorSet() override;
};

} // namespace nx::vms::client::desktop::rules
