#ifndef QN_RECORDED_CHUNKS_REST_HANDLER_H
#define QN_RECORDED_CHUNKS_REST_HANDLER_H

#include <QtCore/QRect>

#include "rest/server/request_handler.h"

class QnRecordedChunksRestHandler: public QnRestRequestHandler
{
	enum ChunkFormat {ChunkFormat_Unknown, ChunkFormat_Binary, ChunkFormat_XML, ChunkFormat_Json, ChunkFormat_Text}; // TODO: Json, not Jason

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, QByteArray& result, QByteArray& contentType);
    virtual QString description() const override;

private:
    QRect deserializeMotionRect(const QString& rectStr);
};


#endif // QN_RECORDED_CHUNKS_REST_HANDLER_H
