
#include "send_statistics_request_data.h"

namespace
{
    const auto kStatServerUrlTag = lit("statUrl");
}

void QnSendStatisticsRequestData::loadFromParams(const QnRequestParamList& params)
{
    if (params.contains(kStatServerUrlTag) )
        statisticsServerUrl = params.value(kStatServerUrlTag);

    Q_ASSERT_X(!statisticsServerUrl.isEmpty(), Q_FUNC_INFO, "Invalid url of statistics server!");
}

QnRequestParamList QnSendStatisticsRequestData::toParams() const
{
    QnRequestParamList result = QnMultiserverRequestData::toParams();
    result.insert(kStatServerUrlTag, statisticsServerUrl);
    return result;
}

