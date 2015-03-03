#ifndef QN_MULTISERVER_CHUNKS_REST_HANDLER_H
#define QN_MULTISERVER_CHUNKS_REST_HANDLER_H

#include <QtCore/QRect>

#include "rest/server/request_handler.h"
#include "rest/server/fusion_rest_handler.h"

class QnMultiserverChunksRestHandler: public QnFusionRestHandler
{
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
};



#endif // QN_MULTISERVER_CHUNKS_REST_HANDLER_H
