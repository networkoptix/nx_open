#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

struct QnWearableCameraReply
{
    QnUuid id;
};
#define QnWearableCameraReply_Fields (id)

QN_FUSION_DECLARE_FUNCTIONS(QnWearableCameraReply, (json)(metatype)(eq))

