// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>

#include "picker_widget.h"

namespace nx::vms::client::desktop::rules {

class VolumePickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    VolumePickerWidget(SystemContext* context, QWidget* parent = nullptr);

private:
    QSlider* m_slider{};
    QPushButton* m_pushButton{};
};

} // namespace nx::vms::client::desktop::rules
