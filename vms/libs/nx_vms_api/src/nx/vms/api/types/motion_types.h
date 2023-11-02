// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>

namespace nx::vms::api {

/**%apidoc
 * Type of motion detection method.
 */
NX_REFLECTION_ENUM_CLASS(MotionType,
    /**%apidoc Use default method.
     * %caption default
     */
    default_ = 0,

    /**%apidoc Use motion detection grid implemented by the camera. */
    hardware = 1 << 0,

    /**%apidoc Use motion detection grid implemented by the server. */
    software = 1 << 1,

    /**%apidoc Use motion detection window implemented by the camera. */
    window = 1 << 2,

    /**%apidoc Do not perform motion detection. */
    none = 1 << 3
);

Q_DECLARE_FLAGS(MotionTypes, MotionType)
Q_DECLARE_OPERATORS_FOR_FLAGS(MotionTypes)

/**%apidoc Index of the stream that is requested from the camera. */
enum class StreamIndex
{
    /**%apidoc[unused] */
    undefined = -1,
    primary = 0, /**<%apidoc High-resolution stream. */
    secondary = 1, /**<%apidoc Low-resolution stream. */
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(StreamIndex*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<StreamIndex>;
    return visitor(
        Item{StreamIndex::undefined, ""},
        Item{StreamIndex::primary, "primary"},
        Item{StreamIndex::secondary, "secondary"}
    );
}

StreamIndex NX_VMS_API oppositeStreamIndex(StreamIndex streamIndex);

} // namespace nx::vms::api
