#ifndef QN_RECORDING_STATS_REST_HANDLER_H
#define QN_RECORDING_STATS_REST_HANDLER_H

#include "rest/server/json_rest_handler.h"


class QnRecordingStatsRestHandler: public QnJsonRestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
};

#endif // QN_RECORDING_STATS_REST_HANDLER_H
