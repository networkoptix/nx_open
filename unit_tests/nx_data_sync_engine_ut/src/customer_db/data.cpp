#include "data.h"

#include <nx/fusion/model_functions.h>

namespace nx::data_sync_engine::test {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Id)(Customer),
    (ubjson)(json),
    _Fields)

} // namespace nx::data_sync_engine::test
