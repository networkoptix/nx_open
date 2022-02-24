// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_type_picker_widget.h"

#include "ui_event_type_picker_widget.h"

#include <ui/common/palette.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

EventTypePickerWidget::EventTypePickerWidget(QWidget* parent /*= nullptr*/):
    base_type(parent),
    m_ui(new Ui::EventTypePickerWidget())
{
    m_ui->setupUi(this);
    setPaletteColor(m_ui->whenLabel, QPalette::WindowText, QPalette().color(QPalette::Light));
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
EventTypePickerWidget::~EventTypePickerWidget()
{
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
