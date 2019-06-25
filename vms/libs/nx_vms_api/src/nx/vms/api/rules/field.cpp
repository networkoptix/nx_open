#include "field.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::rules {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Field, (json),
    nx_vms_api_rules_Field_Fields, (brief, true))

} // namespace nx::vms::api::rules
