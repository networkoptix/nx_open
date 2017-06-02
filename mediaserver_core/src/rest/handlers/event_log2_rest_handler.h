#pragma once

#include <rest/server/fusion_rest_handler.h>

class QnEventLog2RestHandler: public QnFusionRestHandler
{
public:
    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* owner) override;
};
