// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/vms/event/event_fwd.h>

namespace nx::vms::event {

NX_VMS_COMMON_API EventType convertToOldEvent(const QString& typeId);
NX_VMS_COMMON_API QString convertToNewEvent(const EventType& eventType);

} // namespace nx::vms::event
