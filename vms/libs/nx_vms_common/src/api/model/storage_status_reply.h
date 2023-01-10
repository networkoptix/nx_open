// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/model/api_model_fwd.h>

#include <core/resource/resource_fwd.h>

#include <nx/fusion/model_functions_fwd.h>
#include <utils/common/id.h>
#include <common/common_globals.h>
#include <nx/vms/api/data/storage_space_data.h>

/**%apidoc Details about the requested folder. */
struct NX_VMS_COMMON_API QnStorageStatusReply
{
    bool pluginExists;
    nx::vms::api::StorageSpaceData storage;
    Qn::StorageInitResult status;

    QnStorageStatusReply();
};
#define QnStorageStatusReply_Fields (pluginExists)(storage)(status)
QN_FUSION_DECLARE_FUNCTIONS(QnStorageStatusReply, (json)(ubjson), NX_VMS_COMMON_API)

NX_VMS_COMMON_API nx::vms::api::StorageSpaceData fromResourceToApi(
    const QnStorageResourcePtr& storage, bool fastCreate);
