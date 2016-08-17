#pragma once

#include <nx/fusion/model_functions_fwd.h>

struct QnGetNonceReply
{
    QString nonce;
    QString realm;
};
#define QnGetNonceReply_Fields (nonce)(realm)

QN_FUSION_DECLARE_FUNCTIONS(QnGetNonceReply, (json))
Q_DECLARE_METATYPE(QnGetNonceReply);

