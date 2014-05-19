#ifndef CHANGE_SYSTEM_NAME_REST_HANDLER_H
#define CHANGE_SYSTEM_NAME_REST_HANDLER_H

#include "rest/server/request_handler.h"

class QnChangeSystemNameRestHandler : public QnRestRequestHandler {
    Q_OBJECT
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType) override;
    virtual QString description() const override;
};

#endif // CHANGE_SYSTEM_NAME_REST_HANDLER_H
