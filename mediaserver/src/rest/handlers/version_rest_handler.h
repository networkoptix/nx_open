#ifndef QN_VERSION_REST_HANDLER_H
#define QN_VERSION_REST_HANDLER_H

#include "rest/server/request_handler.h"

class QnVersionRestHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType) override;
};

#endif // QN_VERSION_REST_HANDLER_H
