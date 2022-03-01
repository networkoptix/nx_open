// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "picker_widget.h"

namespace Ui { class DurationPickerWidget; }

namespace nx::vms::client::desktop::rules {

class DurationPickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    explicit DurationPickerWidget(QWidget* parent = nullptr);
    virtual ~DurationPickerWidget() override;

    virtual void setReadOnly(bool value) override;

private:
    QScopedPointer<Ui::DurationPickerWidget> ui;

    virtual void onDescriptorSet() override;
};

} // namespace nx::vms::client::desktop::rules
