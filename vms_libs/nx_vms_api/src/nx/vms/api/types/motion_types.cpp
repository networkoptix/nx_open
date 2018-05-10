#include "motion_types.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_DEFINE_FUNCTIONS(nx::vms::api::MotionStreamType, (numeric))
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api, MotionStreamType,
    (nx::vms::api::MotionStreamType::automatic, "auto")
    (nx::vms::api::MotionStreamType::primary, "primary")
    (nx::vms::api::MotionStreamType::secondary, "secondary")
    (nx::vms::api::MotionStreamType::edge, "edge"))
