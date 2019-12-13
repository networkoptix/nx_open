#include "stream_type.h"

#include <nx/fusion/model_functions.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics, StreamType,
    (nx::vms::api::analytics::StreamType::none, "")
    (nx::vms::api::analytics::StreamType::compressedVideo, "compressedVideo")
    (nx::vms::api::analytics::StreamType::uncompressedVideo, "uncompressedVideo")
    (nx::vms::api::analytics::StreamType::metadata, "metadata")
    (nx::vms::api::analytics::StreamType::motion, "motion")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::analytics::StreamType, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::analytics, StreamTypes,
    (nx::vms::api::analytics::StreamType::none, "")
    (nx::vms::api::analytics::StreamType::compressedVideo, "compressedVideo")
    (nx::vms::api::analytics::StreamType::uncompressedVideo, "uncompressedVideo")
    (nx::vms::api::analytics::StreamType::metadata, "metadata")
    (nx::vms::api::analytics::StreamType::motion, "motion")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::analytics::StreamTypes, (numeric)(debug))
