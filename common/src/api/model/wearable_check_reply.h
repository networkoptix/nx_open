#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

struct QnWearableCheckReplyElement
{
    QnTimePeriod period;
};
#define QnWearableCheckReplyElement_Fields (period)

struct QnWearableCheckReply
{
    QVector<QnWearableCheckReplyElement> elements;
    qint64 availableSpace = 0;
};
#define QnWearableCheckReply_Fields (elements)(availableSpace)

QN_FUSION_DECLARE_FUNCTIONS(QnWearableCheckReplyElement, (json)(ubjson)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnWearableCheckReply, (json)(ubjson)(metatype)(eq))
