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
    bool storageCleanupNeeded = false;
    bool storageFull = false;
};
#define QnWearablePrepareReply_Fields (elements)(storageCleanupNeeded)(storageFull)

QN_FUSION_DECLARE_FUNCTIONS(QnWearablePrepareReplyElement, (json)(ubjson)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnWearablePrepareReply, (json)(ubjson)(metatype)(eq))
