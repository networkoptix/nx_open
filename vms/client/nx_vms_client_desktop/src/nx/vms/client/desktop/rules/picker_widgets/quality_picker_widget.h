// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

class QualityPickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    QualityPickerWidget(SystemContext* context, CommonParamsWidget* parent);

private:
    QComboBox* m_comboBox{nullptr};
};

} // namespace nx::vms::client::desktop::rules
