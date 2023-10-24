// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>

namespace nx::vms::api::analytics {

enum class StreamType
{
    /**apidoc[unused] */
    none = 0,
    compressedVideo = 1 << 0, //< Compressed video packets.
    uncompressedVideo = 1 << 1, //< Uncompressed video frames.
    metadata = 1 << 2, //< Metadata that comes in the RTSP stream.
    motion = 1 << 3, //< Motion metadata retrieved by the Server.
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(StreamType*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<StreamType>;
    return visitor(
        Item{StreamType::none, ""},
        Item{StreamType::compressedVideo, "compressedVideo"},
        Item{StreamType::uncompressedVideo, "uncompressedVideo"},
        Item{StreamType::metadata, "metadata"},
        Item{StreamType::motion, "motion"}
    );
}

Q_DECLARE_FLAGS(StreamTypes, StreamType);
Q_DECLARE_OPERATORS_FOR_FLAGS(StreamTypes);

} // namespace nx::vms::api::analytics
