#ifndef QN_TIME_HANDLER_H
#define QN_TIME_HANDLER_H

#include "rest/server/request_handler.h"

class QnTimeHandler: public QnRestRequestHandler
{
public:
    QnTimeHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const;
};

#endif // QN_TIME_HANDLER_H
