// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API IniConfigModel
{
    QnUuid serverId;
    QString iniConfigDir;
};
#define IniConfigModel_Fields (serverId)(iniConfigDir)
QN_FUSION_DECLARE_FUNCTIONS(IniConfigModel, (json), NX_VMS_API)

} // namespace nx::vms::api
