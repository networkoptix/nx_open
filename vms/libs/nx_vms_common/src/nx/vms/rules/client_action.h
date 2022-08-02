// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::rules {

NX_REFLECTION_ENUM_CLASS(ClientAction,
    none,
    poeSettings,
    cameraSettings,
    serverSettings,
    licensesSettings,
    previewCamera,
    previewCameraOnTime,
    browseUrl);

} // namespace nx::vms::api::rules

Q_DECLARE_METATYPE(nx::vms::rules::ClientAction)
