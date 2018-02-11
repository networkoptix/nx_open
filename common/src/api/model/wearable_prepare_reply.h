#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

struct QnWearablePrepareReplyElement
{
    QnTimePeriod period;
};
#define QnWearablePrepareReplyElement_Fields (period)

struct QnWearablePrepareReply
{
    QVector<QnWearablePrepareReplyElement> elements;
    qint64 availableSpace = 0;
};
#define QnWearablePrepareReply_Fields (elements)(availableSpace)

QN_FUSION_DECLARE_FUNCTIONS(QnWearablePrepareReplyElement, (json)(ubjson)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnWearablePrepareReply, (json)(ubjson)(metatype)(eq))
