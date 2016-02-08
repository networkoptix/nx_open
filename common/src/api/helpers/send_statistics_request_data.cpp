
#include "send_statistics_request_data.h"


QnSendStatisticsRequestData::QnSendStatisticsRequestData()
    : QnMultiserverRequestData()
{}

QnSendStatisticsRequestData::~QnSendStatisticsRequestData()
{}

void QnSendStatisticsRequestData::loadFromParams(const QnRequestParamList& params)
{

}

QnRequestParamList QnSendStatisticsRequestData::toParams() const
{
    return QnRequestParamList();
}
