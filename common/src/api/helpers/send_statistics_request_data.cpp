
#include "send_statistics_request_data.h"

#include <nx/utils/log/assert.h>

namespace
{
    const auto kStatServerUrlTag = lit("statUrl");
}

void QnSendStatisticsRequestData::loadFromParams(const QnRequestParamList& params)
{
    if (params.contains(kStatServerUrlTag) )
        statisticsServerUrl = params.value(kStatServerUrlTag);

    NX_ASSERT(!statisticsServerUrl.isEmpty(), Q_FUNC_INFO, "Invalid url of statistics server!");
}

QnRequestParamList QnSendStatisticsRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();
    result.insert(kStatServerUrlTag, statisticsServerUrl);
    return result;
}

