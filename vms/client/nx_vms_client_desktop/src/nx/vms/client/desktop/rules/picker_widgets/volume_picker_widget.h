// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "picker_widget.h"

namespace Ui { class VolumePickerWidget; }

namespace nx::vms::client::desktop::rules {

class VolumePickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    VolumePickerWidget(common::SystemContext* context, QWidget* parent = nullptr);
    virtual ~VolumePickerWidget() override;

    virtual void setReadOnly(bool value) override;

private:
    QScopedPointer<Ui::VolumePickerWidget> ui;

    virtual void onDescriptorSet() override;
};

} // namespace nx::vms::client::desktop::rules
