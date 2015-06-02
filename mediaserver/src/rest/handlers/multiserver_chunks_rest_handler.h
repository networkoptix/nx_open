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
    QnMultiserverChunksRestHandler(const QString& path);
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;

    static MultiServerPeriodDataList loadDataSync(const QnChunksRequestData& request);
private:
    struct InternalContext;
    static void waitForDone(InternalContext* ctx);
    static void loadRemoteDataAsync(MultiServerPeriodDataList& outputData, const QnMediaServerResourcePtr &server, InternalContext* ctx);
    static void loadLocalData(MultiServerPeriodDataList& outputData, InternalContext* ctx);
};



#endif // QN_MULTISERVER_CHUNKS_REST_HANDLER_H
