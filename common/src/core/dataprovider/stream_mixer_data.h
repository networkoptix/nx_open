#pragma once

#include <nx/fusion/model_functions.h>

struct QnChannelMapping
{
    quint32 originalChannel;
    QList<quint32> mappedChannels;
};

struct QnResourceChannelMapping
{
    quint32 resourceChannel;
    QList<QnChannelMapping> channelMap;
};

Q_DECLARE_METATYPE(QnChannelMapping)
Q_DECLARE_METATYPE(QList<QnChannelMapping>)
Q_DECLARE_METATYPE(QnResourceChannelMapping)
Q_DECLARE_METATYPE(QList<QnResourceChannelMapping>)

QN_FUSION_DECLARE_FUNCTIONS(QnChannelMapping, (json))
QN_FUSION_DECLARE_FUNCTIONS(QnResourceChannelMapping, (json))
