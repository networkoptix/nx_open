#ifndef QN_BUSINESS_EVENT_LOG_REST_HANDLER
#define QN_BUSINESS_EVENT_LOG_REST_HANDLER

#include "rest/server/request_handler.h"

class QnBusinessEventLogRestHandler: public QnRestRequestHandler
{
public:
    QnBusinessEventLogRestHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const override;
};

#endif // QN_BUSINESS_EVENT_LOG_REST_HANDLER
