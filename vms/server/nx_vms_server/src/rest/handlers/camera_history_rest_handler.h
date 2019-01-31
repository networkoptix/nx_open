#pragma once

#include <rest/server/request_handler.h>
#include <rest/server/fusion_rest_handler.h>
#include <recording/time_period_list.h>

#include <nx_ec/data/api_fwd.h>
#include <nx/vms/server/server_module_aware.h>

class QnCameraHistoryRestHandler: 
    public QnFusionRestHandler,
    public nx::vms::server::ServerModuleAware
{
public:
    QnCameraHistoryRestHandler(QnMediaServerModule * serverModule);

    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* owner) override;

private:
    friend struct BuildHistoryDataAccess;
    nx::vms::api::CameraHistoryItemDataList buildHistoryData(
        const MultiServerPeriodDataList& chunks);
};
