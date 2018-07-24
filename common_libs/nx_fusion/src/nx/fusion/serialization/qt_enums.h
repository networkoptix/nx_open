#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace Qt {

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Orientation)

} // namespace Qt

QN_FUSION_DECLARE_FUNCTIONS(Qt::Orientations, (numeric)(lexical))
