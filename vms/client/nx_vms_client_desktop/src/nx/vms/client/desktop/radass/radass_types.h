// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/json/qt_containers_reflect.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

NX_REFLECTION_ENUM_CLASS(RadassMode,
    Auto,
    High,
    Low,
    Custom
);

using RadassModeByLayoutItemIdHash = QHash<nx::Uuid, RadassMode>;

} // namespace nx::vms::client::desktop
