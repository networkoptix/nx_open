// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <ostream>

#include <QtCore/QMetaType>
#include <QtCore/QObject>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::api::dewarping {
Q_NAMESPACE_EXPORT(NX_VMS_API)
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

enum class FisheyeCameraMount
{
    wall,
    ceiling,
    table
};
Q_ENUM_NS(FisheyeCameraMount)

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(FisheyeCameraMount*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<FisheyeCameraMount>;
    return visitor(
        Item{FisheyeCameraMount::wall, "wall"},
        Item{FisheyeCameraMount::ceiling, "ceiling"},
        Item{FisheyeCameraMount::table, "table"},
        // Compatibility section.
        Item{FisheyeCameraMount::wall, "Horizontal"},
        Item{FisheyeCameraMount::table , "VerticalUp"},
        Item{FisheyeCameraMount::ceiling, "VerticalDown"}
    );
}

NX_VMS_API std::ostream& operator<<(std::ostream& os, FisheyeCameraMount value);

NX_REFLECTION_ENUM_CLASS(CameraProjection,
    equidistant,
    stereographic,
    equisolid,
    equirectangular360 //< 360-degree panorama stitched by camera.
)
Q_ENUM_NS(CameraProjection)
NX_VMS_API std::ostream& operator<<(std::ostream& os, CameraProjection value);

NX_REFLECTION_ENUM_CLASS(ViewProjection,
    rectilinear,
    equirectangular
)
Q_ENUM_NS(ViewProjection)
NX_VMS_API std::ostream& operator<<(std::ostream& os, ViewProjection value);

} // namespace nx::vms::api::dewarping
