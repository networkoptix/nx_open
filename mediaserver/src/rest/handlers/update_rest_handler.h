#ifndef QN_REST_UPDATE_HANDLER_H
#define QN_REST_UPDATE_HANDLER_H

#include "rest/server/request_handler.h"

class QnUpdateRestHandler : public QnRestRequestHandler {
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType) override;
    virtual QString description() const override;
};

#endif // QN_REST_UPDATE_HANDLER_H
