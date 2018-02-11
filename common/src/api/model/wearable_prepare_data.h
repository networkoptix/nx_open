#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <recording/time_period.h>

struct QnWearableCheckDataElement
{
    qint64 size = 0;
    QnTimePeriod period;
};
#define QnWearableCheckDataElement_Fields (size)(period)

struct QnWearableCheckData
{
    QVector<QnWearableCheckDataElement> elements;
};
#define QnWearableCheckData_Fields (elements)

QN_FUSION_DECLARE_FUNCTIONS(QnWearableCheckDataElement, (json)(ubjson)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnWearableCheckData, (json)(ubjson)(metatype)(eq))
