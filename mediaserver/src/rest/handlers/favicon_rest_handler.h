#ifndef QN_FAVICON_REST_HANDLER_H
#define QN_FAVICON_REST_HANDLER_H

#include "rest/server/request_handler.h"

class QnFavIconRestHandler: public QnRestRequestHandler
{
public:
    QnFavIconRestHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const override;
};

#endif // QN_FAVICON_REST_HANDLER_H
