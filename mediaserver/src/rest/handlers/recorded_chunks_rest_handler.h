#ifndef QN_RECORDED_CHUNKS_REST_HANDLER_H
#define QN_RECORDED_CHUNKS_REST_HANDLER_H

#include <QtCore/QRect>

#include "rest/server/request_handler.h"

class QnRecordedChunksRestHandler: public QnRestRequestHandler
{
    Q_OBJECT
public:

    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType);
    virtual int executePost(const QString& path, const QnRequestParamList& params, const QByteArray& body, const QByteArray& srcBodyContentType, QByteArray& result, QByteArray& contentType);
private:
    // TODO: #Elric #enum
    enum ChunkFormat {
        ChunkFormat_Unknown, 
        ChunkFormat_Binary, 
        ChunkFormat_BinaryIntersected,
        ChunkFormat_XML, 
        ChunkFormat_Json, 
        ChunkFormat_Text
    }; 
};


#endif // QN_RECORDED_CHUNKS_REST_HANDLER_H
