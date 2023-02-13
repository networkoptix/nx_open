// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/rules/action_builder_fields/target_layout_field.h>
#include <nx/vms/rules/action_builder_fields/target_single_device_field.h>

#include "resource_picker_widget_base.h"

namespace nx::vms::client::desktop::rules {

class SingleTargetDevicePicker:
    public ResourcePickerWidgetBase<vms::rules::TargetSingleDeviceField>
{
    Q_OBJECT

public:
    SingleTargetDevicePicker(QnWorkbenchContext* context, CommonParamsWidget* parent);

private:
    QCheckBox* m_checkBox{nullptr};
    vms::rules::TargetLayoutField* m_targetLayoutField{nullptr};

    void onSelectButtonClicked() override;
    void updateUi() override;

    void onStateChanged();
    bool cameraExistOnLayouts(const QnVirtualCameraResourcePtr& camera);
};

} // namespace nx::vms::client::desktop::rules
