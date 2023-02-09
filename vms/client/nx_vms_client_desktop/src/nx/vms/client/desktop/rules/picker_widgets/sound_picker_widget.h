// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>
#include <QtWidgets/QPushButton>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

class SoundPickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    SoundPickerWidget(SystemContext* context, CommonParamsWidget* parent);

private:
    QComboBox* m_comboBox{nullptr};
    QPushButton* m_pushButton{nullptr};
};

} // namespace nx::vms::client::desktop::rules
