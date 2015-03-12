#ifndef QN_MULTISERVER_CHUNKS_REST_HANDLER_H
#define QN_MULTISERVER_CHUNKS_REST_HANDLER_H

#include <QtCore/QRect>

#include "rest/server/request_handler.h"
#include "rest/server/fusion_rest_handler.h"
#include "recording/time_period_list.h"
#include "core/resource/resource_fwd.h"
#include "chunks/chunks_request_helper.h"
#include "utils/network/http/async_http_client_reply.h"
#include "utils/common/systemerror.h"

class QnMultiserverChunksRestHandler: public QnFusionRestHandler
{
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
private:
    struct InternalContext;

    void waitForDone(InternalContext* ctx);
    void loadRemoteDataAsync(MultiServerPeriodDataList& outputData, QnMediaServerResourcePtr server, const QnChunksRequestData& request, InternalContext* ctx);
    void loadLocalData(MultiServerPeriodDataList& outputData, const QnChunksRequestData& request, InternalContext* ctx);
};



#endif // QN_MULTISERVER_CHUNKS_REST_HANDLER_H
