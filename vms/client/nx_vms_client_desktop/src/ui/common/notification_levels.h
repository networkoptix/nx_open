// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QColor>

#include <nx/vms/common/system_health/message_type.h>
#include <nx/vms/event/level.h>

namespace QnNotificationLevel {

nx::vms::event::Level valueOf(
    nx::vms::common::SystemContext* systemContext,
    nx::vms::common::system_health::MessageType messageType);
int priority(nx::vms::common::SystemContext* systemContext,
    nx::vms::common::system_health::MessageType messageType);

QColor notificationTextColor(nx::vms::event::Level level);
QColor notificationColor(nx::vms::event::Level level);

} // namespace QnNotificationLevel
