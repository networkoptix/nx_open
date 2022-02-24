// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "show_notification_action_params_widget.h"

#include "ui_show_notification_action_params_widget.h"

#include <ui/common/palette.h>

namespace nx::vms::client::desktop {
namespace vms_rules {

ShowNotificationActionParamsWidget::ShowNotificationActionParamsWidget(QWidget* parent):
    base_type(parent),
    m_ui(new Ui::ShowNotificationActionParamsWidget())
{
    m_ui->setupUi(this);
    setPaletteColor(
        m_ui->instantEventWarning, QPalette::WindowText, QPalette().color(QPalette::Light));
}

// Non-inline destructor is required for member scoped pointers to forward declared classes.
ShowNotificationActionParamsWidget::~ShowNotificationActionParamsWidget()
{
}

} // namespace vms_rules
} // namespace nx::vms::client::desktop
