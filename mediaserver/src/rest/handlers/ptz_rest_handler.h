#ifndef __PTZ_CONTROL_H__
#define __PTZ_CONTROL_H__

#include "rest/server/request_handler.h"

class QnPtzRestHandler: public QnRestRequestHandler
{
public:
    QnPtzRestHandler();
protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
private:
    bool m_detectAvailableOnly;
};

#endif // __PTZ_CONTROL_H__
