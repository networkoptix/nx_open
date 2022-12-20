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
    SoundPickerWidget(common::SystemContext* context, QWidget* parent = nullptr);

private:
    QComboBox* m_comboBox{};
    QPushButton* m_pushButton{};
};

} // namespace nx::vms::client::desktop::rules
