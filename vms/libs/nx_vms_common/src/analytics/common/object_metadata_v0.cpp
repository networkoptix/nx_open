// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_metadata_v0.h"

#include <nx/fusion/model_functions.h>

namespace nx::common::metadata {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectMetadataPacketV0, (json)(ubjson), ObjectMetadataPacketV0_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectMetadataV0, (json)(ubjson), ObjectMetadataV0_Fields, (brief, true))

} // namespace nx::common::metadata
