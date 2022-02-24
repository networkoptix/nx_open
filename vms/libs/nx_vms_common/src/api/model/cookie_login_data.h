// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/latin1_array.h>

struct QnCookieData
{
    QnLatin1Array auth;
};
#define QnCookieData_Fields (auth)

QN_FUSION_DECLARE_FUNCTIONS(QnCookieData, (json), NX_VMS_COMMON_API)
