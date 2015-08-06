#ifndef QN_AUDIT_LOG_REST_HANDLER_H
#define QN_AUDIT_LOG_REST_HANDLER_H

#include "rest/server/fusion_rest_handler.h"


class QnAuditLogRestHandler: public QnFusionRestHandler
{
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
};

#endif // QN_AUDIT_LOG_REST_HANDLER_H
