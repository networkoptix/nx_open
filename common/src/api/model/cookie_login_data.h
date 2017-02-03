#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/latin1_array.h>

struct QnCookieData
{
    QnLatin1Array auth;
};
#define QnCookieData_Fields (auth)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES( (QnCookieData), (json));
