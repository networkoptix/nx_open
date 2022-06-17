// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "picker_widget.h"

namespace Ui { class PresetPickerWidget; }

namespace nx::vms::client::desktop::rules {

class PresetPickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    PresetPickerWidget(common::SystemContext* context, QWidget* parent = nullptr);
    virtual ~PresetPickerWidget() override;

    virtual void setReadOnly(bool value) override;

private:
    QScopedPointer<Ui::PresetPickerWidget> ui;

    virtual void onDescriptorSet() override;
};

} // namespace nx::vms::client::desktop::rules
