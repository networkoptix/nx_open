// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/storage_space_data.h>

namespace nx::vms::api {

struct NX_VMS_API StorageSpaceReply
{
    std::vector<StorageSpaceDataV1> storages;
    std::vector<std::string> storageProtocols;
};

#define StorageSpaceReply_Fields (storages)(storageProtocols)
NX_REFLECTION_INSTRUMENT(StorageSpaceReply, StorageSpaceReply_Fields)
QN_FUSION_DECLARE_FUNCTIONS(StorageSpaceReply, (json)(ubjson), NX_VMS_API)

} // namespace nx::vms::api
