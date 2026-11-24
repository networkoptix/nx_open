// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/json/flags.h>

namespace nx::vms::api::analytics {

enum class StreamType
{
    /**%apidoc[unused] */
    none = 0,
    compressedVideo = 1 << 0, //< Compressed video packets.
    uncompressedVideo = 1 << 1, //< Uncompressed video frames.
    metadata = 1 << 2, //< Metadata that comes in the RTSP stream.
    motion = 1 << 3, //< Motion metadata retrieved by the Server.
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(StreamType*, Visitor&& visitor)
{
    using EnumItem = nx::reflect::enumeration::Item<StreamType>;
    return visitor(EnumItem{StreamType::none, ""},
        EnumItem{StreamType::compressedVideo, "compressedVideo"},
        EnumItem{StreamType::uncompressedVideo, "uncompressedVideo"},
        EnumItem{StreamType::metadata, "metadata"},
        EnumItem{StreamType::motion, "motion"});
}

Q_DECLARE_FLAGS(StreamTypes, StreamType)
Q_DECLARE_OPERATORS_FOR_FLAGS(StreamTypes)

} // namespace nx::vms::api::analytics
