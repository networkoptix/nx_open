// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "event_action_params_widget.h"

#include "ui_http_action_params_widget.h"

#include <ui/common/palette.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

EventActionParamsWidget::EventActionParamsWidget(QWidget* parent /*= nullptr*/):
    base_type(parent)
{
}

void EventActionParamsWidget::setupLineEditsPlaceholderColor()
{
    const auto lineEdits = findChildren<QLineEdit*>();
    const auto midlightColor = QPalette().color(QPalette::Midlight);

    for (auto lineEdit: lineEdits)
        setPaletteColor(lineEdit, QPalette::PlaceholderText, midlightColor);
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
