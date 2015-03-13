#ifndef QN_CAMERA_HISTORY_REST_HANDLER_H
#define QN_CAMERA_HISTORY_REST_HANDLER_H

#include <QtCore/QRect>

#include "rest/server/request_handler.h"
#include "rest/server/fusion_rest_handler.h"
#include "recording/time_period_list.h"
#include "core/resource/resource_fwd.h"
#include "multiserver_chunks_rest_handler.h"
#include "nx_ec/data/api_camera_server_item_data.h"

class QnCameraHistoryRestHandler: public QnFusionRestHandler
{
public:
    virtual int executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*) override;
private:
    ec2::ApiCameraHistoryMoveDataList buildHistoryData(const MultiServerPeriodDataList& chunks);
};



#endif // QN_CAMERA_HISTORY_REST_HANDLER_H
