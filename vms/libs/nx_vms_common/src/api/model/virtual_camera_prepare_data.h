// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <recording/time_period.h>

struct QnVirtualCameraPrepareDataElement
{
    qint64 size = 0;
    QnTimePeriod period;
};
#define QnVirtualCameraPrepareDataElement_Fields (size)(period)

struct QnVirtualCameraPrepareData
{
    QVector<QnVirtualCameraPrepareDataElement> elements;
};
#define QnVirtualCameraPrepareData_Fields (elements)

QN_FUSION_DECLARE_FUNCTIONS(QnVirtualCameraPrepareDataElement, (json)(ubjson))
QN_FUSION_DECLARE_FUNCTIONS(QnVirtualCameraPrepareData, (json)(ubjson), NX_VMS_COMMON_API)
