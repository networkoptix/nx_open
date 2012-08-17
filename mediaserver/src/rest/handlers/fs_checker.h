#ifndef __FS_CHECKER_H__
#define __FS_CHECKER_H__

#include "rest/server/request_handler.h"

class QnFsHelperHandler: public QnRestRequestHandler
{
public:
    QnFsHelperHandler(bool detectAvailableOnly);
protected:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
private:
    qint64 parseDateTime(const QString& dateTime);
    QRect deserializeMotionRect(const QString& rectStr);
private:
    bool m_detectAvailableOnly;
};

#endif // __FS_CHECKER_H__
