// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include "update_information.h"

namespace nx::vms::common::update {

struct NX_VMS_COMMON_API StartUpdateReply
{
    Information info;
    QList<QnUuid> persistentStorageServers;
};

#define StartUpdateReply_Fields \
    (info) \
    (persistentStorageServers)

QN_FUSION_DECLARE_FUNCTIONS(StartUpdateReply, (json), NX_VMS_COMMON_API)

} // namespace nx::vms::common::update
