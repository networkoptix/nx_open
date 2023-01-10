// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "stream_mixer_data.h"
#include <nx/fusion/model_functions.h>

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnChannelMapping, (json), (originalChannel)(mappedChannel))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnResourceChannelMapping, (json), (resourceChannel)(channelMap))
