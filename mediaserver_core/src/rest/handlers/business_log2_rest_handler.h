#ifndef QN_BUSINESS_LOG2_REST_HANDLER_H
#define QN_BUSINESS_LOG2_REST_HANDLER_H

#include "rest/server/fusion_rest_handler.h"


class QnBusinessLog2RestHandler: public QnFusionRestHandler
{
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
};

#endif // QN_BUSINESS_LOG2_REST_HANDLER_H
