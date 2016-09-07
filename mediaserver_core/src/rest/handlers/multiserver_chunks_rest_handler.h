#pragma once

#include <rest/server/fusion_rest_handler.h>
#include <api/helpers/request_helpers_fwd.h>
#include <recording/time_period_list.h>

class QnMultiserverChunksRestHandler: public QnFusionRestHandler
{
public:
    QnMultiserverChunksRestHandler(const QString& path);

    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* /*owner*/) override;

    static MultiServerPeriodDataList loadDataSync(
        const QnChunksRequestData& request, const QnRestConnectionProcessor* owner);
};
