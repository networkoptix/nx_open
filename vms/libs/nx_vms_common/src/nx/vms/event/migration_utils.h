// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QString>

#include <nx/vms/event/event_fwd.h>

namespace nx::vms::event {

// Use convertToOld* functions only for testing or backwards compatibility.

NX_VMS_COMMON_API EventType convertToOldEvent(const QString& typeId);
NX_VMS_COMMON_API QString convertToNewEvent(EventType eventType);

NX_VMS_COMMON_API ActionType convertToOldAction(const QString& typeId);
NX_VMS_COMMON_API QString convertToNewAction(ActionType actionType);

} // namespace nx::vms::event
