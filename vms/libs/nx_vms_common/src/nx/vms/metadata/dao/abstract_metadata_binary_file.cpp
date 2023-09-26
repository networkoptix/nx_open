// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <cstdint>
#include <string>

#include "abstract_metadata_binary_file.h"
#include "qfile_metadata_binary_file.h"

namespace nx::vms::metadata {

MetadataBinaryFileFactory::MetadataBinaryFileFactory(): base_type([]() { return std::make_unique<QFileMetadataBinaryFile>(); })
{
}

MetadataBinaryFileFactory& MetadataBinaryFileFactory::instance()
{
    static MetadataBinaryFileFactory staticInstance;
    return staticInstance;
}

} // namespace nx::vms::metadata
