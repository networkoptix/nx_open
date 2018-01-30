#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

struct QnWearableStatusReply
{
    bool success = false;
    bool locked = false;
    bool consuming = false;
    QnUuid userId;
    QnUuid token;
    int progress = 0;
};
#define QnWearableStatusReply_Fields (success)(locked)(consuming)(userId)(token)(progress)

QN_FUSION_DECLARE_FUNCTIONS(QnWearableStatusReply, (json)(ubjson)(metatype)(eq))

