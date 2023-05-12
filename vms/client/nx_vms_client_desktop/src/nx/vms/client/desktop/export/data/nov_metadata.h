// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>

namespace nx::vms::client::desktop {

/** Metadata file, describing exported nov-file contents. */
struct NovMetadata
{
    static constexpr int kVersion51 = 51;

    int version = 0;
};
#define NovMetadata_Fields (version)

NX_REFLECTION_INSTRUMENT(NovMetadata, NovMetadata_Fields)

} // namespace nx::vms::client::desktop