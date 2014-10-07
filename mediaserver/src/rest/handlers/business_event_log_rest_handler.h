#ifndef QN_BUSINESS_EVENT_LOG_REST_HANDLER
#define QN_BUSINESS_EVENT_LOG_REST_HANDLER

#include "rest/server/request_handler.h"

class QnBusinessEventLogRestHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, 
                            QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
};

#endif // QN_BUSINESS_EVENT_LOG_REST_HANDLER
