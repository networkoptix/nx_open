#ifndef __FS_CHECKER_H__
#define __FS_CHECKER_H__

#include "rest/server/request_handler.h"

class QnFsHelperHandler: public QnRestRequestHandler
{
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result);
    virtual QString description(TCPSocket* tcpSocket) const;
private:
    qint64 parseDateTime(const QString& dateTime);
    QRect deserializeMotionRect(const QString& rectStr);
};

#endif // __FS_CHECKER_H__
