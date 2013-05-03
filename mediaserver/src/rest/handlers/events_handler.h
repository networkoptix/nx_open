#ifndef QN_REST_EVENTS__HANDLER_H
#define QN_REST_EVENTS__HANDLER_H

#include "rest/server/request_handler.h"

class QnRestEventsHandler: public QnRestRequestHandler
{
public:
    QnRestEventsHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const;
};

#endif // QN_REST_EVENTS__HANDLER_H
