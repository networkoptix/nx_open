// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/common/widgets/slide_switch.h>

#include "picker_widget.h"

class QLabel;
class QWidget;

namespace nx::vms::client::desktop::rules {

class TitledPickerWidget: public PickerWidget
{
    Q_OBJECT

public:
    TitledPickerWidget(SystemContext* context, CommonParamsWidget* parent);

    void setCheckBoxEnabled(bool value);
    void setChecked(bool value);
    bool isChecked() const;

    void setReadOnly(bool value) override;

protected:
    QLabel* m_title{nullptr};
    SlideSwitch* m_enabledSwitch{nullptr};
    QWidget* m_contentWidget{nullptr};

    virtual void onEnabledChanged(bool isEnabled);

    void onDescriptorSet() override;
};

} // namespace nx::vms::client::desktop::rules
