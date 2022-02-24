// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "action_type_picker_widget.h"

#include "ui_action_type_picker_widget.h"

#include <ui/common/palette.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

ActionTypePickerWidget::ActionTypePickerWidget(QWidget* parent /*= nullptr*/):
    base_type(parent),
    m_ui(new Ui::ActionTypePickerWidget())
{
    m_ui->setupUi(this);
    setPaletteColor(m_ui->doLabel, QPalette::WindowText, QPalette().color(QPalette::Light));
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
ActionTypePickerWidget::~ActionTypePickerWidget()
{
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
