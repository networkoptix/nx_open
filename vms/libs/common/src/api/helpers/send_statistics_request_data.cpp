
#include "send_statistics_request_data.h"

#include <nx/utils/log/assert.h>

namespace
{
    const auto kStatServerUrlTag = lit("statUrl");
}

void QnSendStatisticsRequestData::loadFromParams(QnResourcePool* resourcePool,
    const QnRequestParamList& params)
{
    QnMultiserverRequestData::loadFromParams(resourcePool, params);

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

