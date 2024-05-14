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
    TitledPickerWidget(const QString& displayName, SystemContext* context, ParamsWidget* parent);

    void setCheckBoxEnabled(bool value);
    void setChecked(bool value);
    bool isChecked() const;
    void setReadOnly(bool value) override;

    virtual void setValidity(const vms::rules::ValidationResult& validationResult);

protected:
    QLabel* m_title{nullptr};
    SlideSwitch* m_enabledSwitch{nullptr};
    QWidget* m_contentWidget{nullptr};
    QLabel* m_alertLabel{nullptr};

    virtual void onEnabledChanged(bool isEnabled);
};

} // namespace nx::vms::client::desktop::rules
