#ifndef __RECORDED_CHUNKS_H__
#define __RECORDED_CHUNKS_H__

#include <QRect>
#include "rest/server/request_handler.h"

class QnRecordedChunkListHandler: public QnRestRequestHandler
{
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
private:
    qint64 parseDateTime(const QString& dateTime);
    QRect deserializeMotionRect(const QString& rectStr);
};

class QnXsdHelperHandler: public QnRestRequestHandler
{
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
};

#endif // __RECORD_CHUNKS_H__
