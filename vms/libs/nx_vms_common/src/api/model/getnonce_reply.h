// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

struct NX_VMS_COMMON_API QnGetNonceReply
{
    QString nonce;
    QString realm;
};
#define QnGetNonceReply_Fields (nonce)(realm)

QN_FUSION_DECLARE_FUNCTIONS(QnGetNonceReply, (json)(ubjson), NX_VMS_COMMON_API)
