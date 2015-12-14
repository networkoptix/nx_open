#pragma once

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <rest/server/fusion_rest_handler.h>

#include <api/helpers/request_helpers_fwd.h>

class QnMultiserverChunksRestHandler: public QnFusionRestHandler
{
public:
    QnMultiserverChunksRestHandler(const QString& path);
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;

    static MultiServerPeriodDataList loadDataSync(const QnChunksRequestData& request, const QnRestConnectionProcessor* owner);
};
