#include "persistent_update_storage.h"
#include <nx/fusion/model_functions.h>

namespace nx::update {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    nx::update::PersistentUpdateStorage, (ubjson)(json)(datastream)(eq),
    PersistentUpdateStorage_Fields)

} // namespace nx::update
