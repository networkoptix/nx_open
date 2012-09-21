#ifndef QN_PTZ_HANDLER_H
#define QN_PTZ_HANDLER_H

#include "rest/server/request_handler.h"

class QnPtzHandler: public QnRestRequestHandler
{
public:
    QnPtzHandler();
protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
private:
    bool m_detectAvailableOnly;
};

#endif // QN_PTZ_HANDLER_H
