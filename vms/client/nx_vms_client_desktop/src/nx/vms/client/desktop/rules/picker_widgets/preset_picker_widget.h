// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QComboBox>

#include "plain_picker_widget.h"

namespace nx::vms::client::desktop::rules {

class PresetPickerWidget: public PlainPickerWidget
{
    Q_OBJECT

public:
    PresetPickerWidget(SystemContext* context, CommonParamsWidget* parent);

    void updateUi() override;

private:
    QComboBox* m_comboBox{nullptr};
};

} // namespace nx::vms::client::desktop::rules
