// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "live_stream_params.h"
#include <nx/fusion/model_functions.h>

const float QnLiveStreamParams::kFpsNotInitialized = -1.0;

QString QnLiveStreamParams::toString() const
{
    return QJson::serialized(*this);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnLiveStreamParams, (json), QnLiveStreamParams_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnAdvancedStreamParams, (json), QnAdvancedStreamParams_Fields)
