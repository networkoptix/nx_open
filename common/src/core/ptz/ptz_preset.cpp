#include "ptz_preset.h"

#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzPreset, (json)(eq), QnPtzPreset_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzPresetData, (json)(eq), QnPtzPresetData_Fields);
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnPtzPresetRecord, (json)(eq), QnPtzPresetRecord_Fields);

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::core::ptz, PresetType,
    (nx::core::ptz::PresetType::undefined, "undefined")
    (nx::core::ptz::PresetType::system, "system")
    (nx::core::ptz::PresetType::native, "native")
);
