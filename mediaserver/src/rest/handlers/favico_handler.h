#ifndef QN_FAV_ICON__HANDLER_H
#define QN_FAV_ICON__HANDLER_H

#include "rest/server/request_handler.h"

class QnRestFavicoHandler: public QnRestRequestHandler
{
public:
    QnRestFavicoHandler();

protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
};

#endif // QN_FAV_ICON__HANDLER_H
