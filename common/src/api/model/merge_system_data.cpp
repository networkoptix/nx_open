#include "merge_system_data.h"

#include <nx/fusion/model_functions.h>

MergeSystemData::MergeSystemData(const QnRequestParams& params):
    url(params.value(lit("url"))),
    getKey(params.value(lit("getKey"))),
    postKey(params.value(lit("postKey"))),
    takeRemoteSettings(params.value(lit("takeRemoteSettings"), lit("false")) != lit("false")),
    mergeOneServer(params.value(lit("oneServer"), lit("false")) != lit("false")),
    ignoreIncompatible(params.value(lit("ignoreIncompatible"), lit("false")) != lit("false"))
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    MergeSystemData, (json),
    (url)(getKey)(postKey)(takeRemoteSettings)(mergeOneServer)(ignoreIncompatible))
