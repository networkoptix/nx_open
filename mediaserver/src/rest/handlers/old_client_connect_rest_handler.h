#ifndef OLD_CLIENT_CONNECT_HANDLER_H
#define OLD_CLIENT_CONNECT_HANDLER_H

#include "rest/server/request_handler.h"

//!Receives events from cameras (Acti at the moment) and dispatches it to corresponding resource
class QnOldClientConnectRestHandler
:
    public QnRestRequestHandler
{
    Q_OBJECT
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& responseMessageBody, QByteArray& contentType) override;
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& requestBody, const QByteArray&, QByteArray& responseMessageBody, QByteArray& contentType) override;
};

#endif // OLD_CLIENT_CONNECT_HANDLER_H
