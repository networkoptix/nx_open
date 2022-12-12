// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QtGlobal>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM(StorageStatus,
    none = 0,
    used = 1 << 1,
    tooSmall = 1 << 2,
    system = 1 << 3,
    removable = 1 << 4,
    beingChecked = 1 << 5,
    beingRebuilt = 1 << 6,
    disabled = 1 << 7,
    dbReady = 1 << 8
)

Q_DECLARE_FLAGS(StorageStatuses, StorageStatus)
Q_DECLARE_OPERATORS_FOR_FLAGS(StorageStatuses)

} // namespace nx::vms::api
