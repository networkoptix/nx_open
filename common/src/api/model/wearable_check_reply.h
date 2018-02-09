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
};
#define QnWearableCheckReply_Fields (elements)

QN_FUSION_DECLARE_FUNCTIONS(QnWearableCheckReplyElement, (json)(ubjson)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnWearableCheckReply, (json)(ubjson)(metatype)(eq))
