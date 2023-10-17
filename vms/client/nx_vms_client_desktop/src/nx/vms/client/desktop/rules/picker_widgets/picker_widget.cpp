// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/rules/params_widgets/common_params_widget.h>

namespace nx::vms::client::desktop::rules {

using Field = vms::rules::Field;
using FieldDescriptor = vms::rules::FieldDescriptor;

PickerWidget::PickerWidget(SystemContext* context, CommonParamsWidget* parent):
    QWidget(parent),
    SystemContextAware(context)
{
}

void PickerWidget::setDescriptor(const FieldDescriptor& descriptor)
{
    m_fieldDescriptor = descriptor;

    onDescriptorSet();
}

std::optional<FieldDescriptor> PickerWidget::descriptor() const
{
    return m_fieldDescriptor;
}

bool PickerWidget::hasDescriptor() const
{
    return m_fieldDescriptor != std::nullopt;
}

CommonParamsWidget* PickerWidget::parentParamsWidget() const
{
    return dynamic_cast<CommonParamsWidget*>(parent());
}

} // namespace nx::vms::client::desktop::rules
