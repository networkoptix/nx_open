#ifndef QN_EXTERNAL_BUSINESS_EVENT_HANDLER_H
#define QN_EXTERNAL_BUSINESS_EVENT_HANDLER_H

#include "rest/server/request_handler.h"

class QnExternalBusinessEventHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:
    QnExternalBusinessEventHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, QByteArray& contentEncoding);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType, QByteArray& contentEncoding);
    virtual QString description(TCPSocket* tcpSocket) const;
};

#endif // QN_FILE_SYSTEM_HANDLER_H
