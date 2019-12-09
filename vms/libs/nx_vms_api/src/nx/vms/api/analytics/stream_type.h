#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::analytics {

enum class StreamType
{
    none = 0,
    compressedVideo = 1 << 0,
    uncompressedVideo = 1 << 1,
    metadata = 1 << 2,
    motion = 1 << 3,
};

Q_DECLARE_FLAGS(StreamTypes, StreamType);
Q_DECLARE_OPERATORS_FOR_FLAGS(StreamTypes);
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(StreamType);

} // namespace nx::vms::api::analytics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::StreamType,
    (metatype)(numeric)(lexical), NX_VMS_API)

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::analytics::StreamTypes,
    (metatype)(numeric)(lexical), NX_VMS_API)
