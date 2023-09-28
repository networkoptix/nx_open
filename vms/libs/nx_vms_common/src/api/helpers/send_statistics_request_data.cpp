// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "send_statistics_request_data.h"

#include <nx/utils/log/assert.h>
#include <nx/network/rest/params.h>

namespace
{
    const auto kStatServerUrlTag = "statUrl";
}

void QnSendStatisticsRequestData::loadFromParams(
    QnResourcePool* resourcePool, const nx::network::rest::Params& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);

    if (params.contains(kStatServerUrlTag) )
        statisticsServerUrl = params.value(kStatServerUrlTag);

    NX_ASSERT(!statisticsServerUrl.isEmpty(), "Invalid url of statistics server!");
}

nx::network::rest::Params QnSendStatisticsRequestData::toParams() const
{
    auto result = QnMultiserverRequestData::toParams();
    result.insert(kStatServerUrlTag, statisticsServerUrl);
    return result;
}
