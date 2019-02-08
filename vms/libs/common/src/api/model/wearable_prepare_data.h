#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>
#include <recording/time_period.h>

struct QnWearablePrepareDataElement
{
    qint64 size = 0;
    QnTimePeriod period;
};
#define QnWearablePrepareDataElement_Fields (size)(period)

struct QnWearablePrepareData
{
    QVector<QnWearablePrepareDataElement> elements;
};
#define QnWearablePrepareData_Fields (elements)

QN_FUSION_DECLARE_FUNCTIONS(QnWearablePrepareDataElement, (json)(ubjson)(metatype)(eq))
QN_FUSION_DECLARE_FUNCTIONS(QnWearablePrepareData, (json)(ubjson)(metatype)(eq))
