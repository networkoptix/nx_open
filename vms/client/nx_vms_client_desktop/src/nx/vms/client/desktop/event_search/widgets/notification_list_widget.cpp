// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_list_widget.h"
#include "private/notification_list_widget_p.h"

namespace nx::vms::client::desktop {

NotificationListWidget::NotificationListWidget(QWidget* parent):
    QWidget(parent),
    d(new Private(this))
{
}

NotificationListWidget::~NotificationListWidget()
{
}

} // namespace nx::vms::client::desktop
