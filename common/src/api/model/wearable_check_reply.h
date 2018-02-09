#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

struct QnWearableCheckReplyElement
{
    qint64 startTimeMs = 0;
    qint64 durationMs = 0;
};
#define QnWearableCheckReplyElement_Fields (startTimeMs)(durationMs)

struct QnWearableCheckReply
{
    QVector<QnWearableCheckReplyElement> elements;
};
#define QnWearableCheckReply_Fields (elements)

QN_FUSION_DECLARE_FUNCTIONS(QnWearableCheckReplyElement, (json)(ubjson)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnWearableCheckReply, (json)(ubjson)(metatype)(eq))
