// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "picker_widget.h"

namespace Ui { class SoundPickerWidget; }

namespace nx::vms::client::desktop::rules {

class SoundPickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    SoundPickerWidget(common::SystemContext* context, QWidget* parent = nullptr);
    virtual ~SoundPickerWidget() override;

    virtual void setReadOnly(bool value) override;

private:
    QScopedPointer<Ui::SoundPickerWidget> ui;

    virtual void onDescriptorSet() override;
};

} // namespace nx::vms::client::desktop::rules
