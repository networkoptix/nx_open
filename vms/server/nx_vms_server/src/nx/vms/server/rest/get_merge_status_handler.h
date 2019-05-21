#pragma once

#include <rest/server/json_rest_handler.h>
#include <rest/helpers/request_context.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/api/data/merge_status_reply.h>
#include <core/resource/resource_fwd.h>
#include <rest/helpers/request_helpers.h>

namespace nx::vms::server::rest {

struct MergeStatusRequestData: public QnMultiserverRequestData
{
    MergeStatusRequestData() {}
};

class GetMergeStatusHandler:
    public QnJsonRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    static QString kUrlPath;

    explicit GetMergeStatusHandler(
        QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;
private:
    void loadRemoteDataAsync(
        std::vector<nx::vms::api::MergeStatusReply>& outputData,
        const QnMediaServerResourcePtr& server,
        QnMultiserverRequestContext<MergeStatusRequestData>* ctx) const;
};

} // namespace nx::vms::server::rest