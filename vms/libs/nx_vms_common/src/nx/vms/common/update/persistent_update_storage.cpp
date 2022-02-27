// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "persistent_update_storage.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::common::update {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    PersistentUpdateStorage, (ubjson)(json)(datastream),
    PersistentUpdateStorage_Fields)

} // namespace nx::vms::common::update
