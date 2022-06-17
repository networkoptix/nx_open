// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "picker_widget.h"

namespace Ui { class QualityPickerWidget; }

namespace nx::vms::client::desktop::rules {

class QualityPickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    QualityPickerWidget(common::SystemContext* context, QWidget* parent = nullptr);
    virtual ~QualityPickerWidget() override;

    virtual void setReadOnly(bool value) override;

private:
    QScopedPointer<Ui::QualityPickerWidget> ui;

    virtual void onDescriptorSet() override;
};

} // namespace nx::vms::client::desktop::rules
