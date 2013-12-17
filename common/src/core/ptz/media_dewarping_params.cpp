#include "media_dewarping_params.h"

#include <utils/common/model_functions.h>

QN_DEFINE_ENUM_CAST_LEXICAL_JSON_SERIALIZATION_FUNCTIONS(QnMediaDewarpingParams::ViewMode, static)
QN_DEFINE_STRUCT_FUNCTIONS(QnMediaDewarpingParams,      (json)(eq),    (enabled)(viewMode)(fovRot))
