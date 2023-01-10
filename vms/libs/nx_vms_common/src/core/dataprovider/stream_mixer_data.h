// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>

struct QnChannelMapping
{
    quint32 originalChannel = 0;
    quint32 mappedChannel = 0;
};

struct QnResourceChannelMapping
{
    quint32 resourceChannel;
    QList<QnChannelMapping> channelMap;
};

QN_FUSION_DECLARE_FUNCTIONS(QnChannelMapping, (json))
QN_FUSION_DECLARE_FUNCTIONS(QnResourceChannelMapping, (json))
