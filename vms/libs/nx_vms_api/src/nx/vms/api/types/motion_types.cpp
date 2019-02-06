#include "motion_types.h"

#include <nx/fusion/model_functions.h>

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

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, MotionStreamType,
    (nx::vms::api::MotionStreamType::undefined, "")
    (nx::vms::api::MotionStreamType::primary, "primary")
    (nx::vms::api::MotionStreamType::secondary, "secondary")
)
QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::MotionStreamType, (numeric)(debug))
