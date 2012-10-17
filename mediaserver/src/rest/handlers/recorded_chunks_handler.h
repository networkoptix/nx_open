#ifndef QN_RECORDED_CHUNKS_HANDLER_H
#define QN_RECORDED_CHUNKS_HANDLER_H

#include <QtCore/QRect>

#include "rest/server/request_handler.h"

class QnRecordedChunksHandler: public QnRestRequestHandler
{
	enum ChunkFormat {ChunkFormat_Unknown, ChunkFormat_Binary, ChunkFormat_XML, ChunkFormat_Jason, ChunkFormat_Text};

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description(TCPSocket* tcpSocket) const;
private:
    QRect deserializeMotionRect(const QString& rectStr);
};

class QnXsdHelperHandler: public QnRestRequestHandler
{
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
};

#endif // QN_RECORDED_CHUNKS_HANDLER_H
