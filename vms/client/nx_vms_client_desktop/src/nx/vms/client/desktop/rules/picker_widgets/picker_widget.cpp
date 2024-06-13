// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_widget.h"

#include <QtWidgets/QHBoxLayout>

#include <nx/vms/client/desktop/rules/params_widgets/params_widget.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop::rules {

using Field = vms::rules::Field;
using FieldDescriptor = vms::rules::FieldDescriptor;

PickerWidget::PickerWidget(SystemContext* context, ParamsWidget* parent):
    QWidget(parent),
    SystemContextAware(context)
{
}

bool PickerWidget::isEdited() const
{
    return m_isEdited;
}

void PickerWidget::setEdited()
{
    m_isEdited = true;
}

ParamsWidget* PickerWidget::parentParamsWidget() const
{
    return dynamic_cast<ParamsWidget*>(parent());
}

void PickerWidget::setValidity(const vms::rules::ValidationResult& /*validationResult*/)
{
}

} // namespace nx::vms::client::desktop::rules
