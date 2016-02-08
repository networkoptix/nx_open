
#pragma once

#include <api/helpers/multiserver_request_data.h>

class QnSendStatisticsRequestData : public QnMultiserverRequestData
{
public:
    QnSendStatisticsRequestData();

    virtual ~QnSendStatisticsRequestData();

    virtual void loadFromParams(const QnRequestParamList& params);

    virtual QnRequestParamList toParams() const;
};
