#include "motion_types.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

StreamIndex oppositeStreamIndex(StreamIndex streamIndex)
{
    switch (streamIndex)
    {
        case StreamIndex::primary:
            return StreamIndex::secondary;
        case StreamIndex::secondary:
            return StreamIndex::primary;
        default:
            break;
    }
    NX_ASSERT(false, lm("Unsupported StreamIndex %1").args(streamIndex));
    return StreamIndex::undefined; //< Fallback for the failed assertion.
}

} // namespace nx::vms::api

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, MotionType,
    (nx::vms::api::MT_Default, "MT_Default")
    (nx::vms::api::MT_HardwareGrid, "MT_HardwareGrid")
    (nx::vms::api::MT_SoftwareGrid, "MT_SoftwareGrid")
    (nx::vms::api::MT_MotionWindow, "MT_MotionWindow")
    (nx::vms::api::MT_NoMotion, "MT_NoMotion")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::MotionType, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, MotionTypes,
    (nx::vms::api::MT_Default, "MT_Default")
    (nx::vms::api::MT_HardwareGrid, "MT_HardwareGrid")
    (nx::vms::api::MT_SoftwareGrid, "MT_SoftwareGrid")
    (nx::vms::api::MT_MotionWindow, "MT_MotionWindow")
    (nx::vms::api::MT_NoMotion, "MT_NoMotion")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::MotionTypes, (numeric)(debug))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, StreamIndex,
    (nx::vms::api::StreamIndex::undefined, "")
    (nx::vms::api::StreamIndex::primary, "primary")
    (nx::vms::api::StreamIndex::secondary, "secondary")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::StreamIndex, (numeric)(debug))
