#ifndef QN_VERSION_HANDLER_H
#define QN_VERSION_HANDLER_H

#include "rest/server/request_handler.h"

class QnVersionRestHandler: public QnRestRequestHandler
{
public:
protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType) override;
    virtual QString description() const override;
};

#endif // QN_VERSION_HANDLER_H
