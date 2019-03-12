#pragma once

#include <QtCore/QElapsedTimer>

#include <rest/server/fusion_rest_handler.h>
#include <api/helpers/request_helpers_fwd.h>
#include <recording/time_period_list.h>
#include <api/helpers/chunks_request_data.h>
#include <rest/helpers/request_context.h>
#include <nx/vms/server/server_module_aware.h>

typedef QnMultiserverRequestContext<QnChunksRequestData> QnChunksRequestContext;

class QnMultiserverChunksRestHandler:
    public QnFusionRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    static const QString kUrlPath;

    QnMultiserverChunksRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString& path, const QnRequestParamList& params, QByteArray& result,
        QByteArray& contentType, const QnRestConnectionProcessor* /*owner*/) override;

    MultiServerPeriodDataList loadDataSync(
        const QnChunksRequestData& request,
        const QnRestConnectionProcessor* owner) const;

    static MultiServerPeriodDataList mergeDataWithSameId(
        const MultiServerPeriodDataList& periodList,
        int limit,
        Qt::SortOrder sortOrder);
private:
    MultiServerPeriodDataList loadDataSync(
        const QnChunksRequestData& request,
        const QnRestConnectionProcessor* owner,
        int requestNum,
        QElapsedTimer& timer) const;

    void loadRemoteDataAsync(
        MultiServerPeriodDataList& outputData,
        const QnMediaServerResourcePtr& server,
        QnChunksRequestContext* ctx,
        int requestNum,
        const QElapsedTimer& timer) const;

    void loadLocalData(
        MultiServerPeriodDataList& outputData,
        QnChunksRequestContext* ctx) const;
};
