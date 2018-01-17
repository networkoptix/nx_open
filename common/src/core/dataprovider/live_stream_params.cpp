#include "live_stream_params.h"
#include <nx/fusion/model_functions.h>

const float QnLiveStreamParams::kFpsNotInitialized = -1.0;
const int QnLiveStreamParams::kMinSecondStreamFps; //< Workaround for GCC 4.8.2 bug.

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnLiveStreamParams, (eq)(json), QnLiveStreamParams_Fields)
