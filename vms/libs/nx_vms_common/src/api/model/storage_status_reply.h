// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/data/storage_space_data.h>

/**%apidoc Details about the requested folder. */
struct NX_VMS_COMMON_API StorageStatusReply
{
    bool pluginExists = false;
    nx::vms::api::StorageSpaceDataV1 storage;
    Qn::StorageInitResult status = Qn::StorageInit_CreateFailed;
};
#define StorageStatusReply_Fields (pluginExists)(storage)(status)
NX_REFLECTION_INSTRUMENT(StorageStatusReply, StorageStatusReply_Fields)
QN_FUSION_DECLARE_FUNCTIONS(StorageStatusReply, (json)(ubjson), NX_VMS_COMMON_API)
