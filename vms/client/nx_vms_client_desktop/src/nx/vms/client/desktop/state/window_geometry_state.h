// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QRect>

#include <nx/reflect/instrument.h>
#include <nx/utils/json/qt_geometry_reflect.h>

namespace nx::vms::client::desktop {

struct WindowGeometryState
{
    QRect geometry;

    // TODO: #spanasenko Use enum / flags here?
    bool isFullscreen = false;
    bool isMaximized = false;

    bool operator==(const WindowGeometryState&) const = default;
};
#define WindowGeometryState_Fields (geometry)(isFullscreen)(isMaximized)

NX_REFLECTION_INSTRUMENT(WindowGeometryState, WindowGeometryState_Fields)

} // namespace nx::vms::client::desktop
