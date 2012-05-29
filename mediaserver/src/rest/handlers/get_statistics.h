#ifndef __REST_GET_STATISTICS_H__
#define __REST_GET_STATISTICS_H__

#include "rest/server/request_handler.h"

class QnGetStatisticsHandler: public QnRestRequestHandler
{
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result);
    virtual QString description(TCPSocket* tcpSocket) const;
private:
    qint64 parseDateTime(const QString& dateTime);
    QRect deserializeMotionRect(const QString& rectStr);
};

#endif // __REST_GET_STATISTICS_H__
