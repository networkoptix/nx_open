// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QtGlobal>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api {

NX_REFLECTION_ENUM_CLASS(StorageRuntimeFlag,
    none = 0,
    used = 1 << 1,
    tooSmall = 1 << 2,
    beingChecked = 1 << 3,
    beingRebuilt = 1 << 4,
    disabled = 1 << 5
)

Q_DECLARE_FLAGS(StorageRuntimeFlags, StorageRuntimeFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(StorageRuntimeFlags)

NX_REFLECTION_ENUM_CLASS(StoragePersistentFlag,
    none = 0,
    system = 1 << 1,
    removable = 1 << 2,
    dbReady = 1 << 3
)

Q_DECLARE_FLAGS(StoragePersistentFlags, StoragePersistentFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(StoragePersistentFlags)

} // namespace nx::vms::api
