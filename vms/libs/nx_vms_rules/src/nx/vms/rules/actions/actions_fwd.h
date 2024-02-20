// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

namespace nx::vms::rules {

class NotificationActionBase;
using NotificationActionBasePtr = QSharedPointer<NotificationActionBase>;

class NotificationAction;
using NotificationActionPtr = QSharedPointer<NotificationAction>;

} // namespace nx::vms::rules
