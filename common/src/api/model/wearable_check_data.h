#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

struct QnWearableCheckDataElement
{
    qint64 size = 0;
    qint64 startTimeMs = 0;
    qint64 durationMs = 0;
};
#define QnWearableCheckDataElement_Fields (size)(startTimeMs)(durationMs)

struct QnWearableCheckData
{
    QVector<QnWearableCheckDataElement> elements;
};
#define QnWearableCheckData_Fields (elements)

QN_FUSION_DECLARE_FUNCTIONS(QnWearableCheckDataElement, (json)(ubjson)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnWearableCheckData, (json)(ubjson)(metatype)(eq))
