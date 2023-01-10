// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

struct QnConfigureReply
{
    QnConfigureReply(): restartNeeded(false) {}

    /**%apidoc Shows whether the Server must be restarted to apply settings. */
    bool restartNeeded;
};
#define QnConfigureReply_Fields (restartNeeded)
QN_FUSION_DECLARE_FUNCTIONS(QnConfigureReply, (json), NX_VMS_COMMON_API)
