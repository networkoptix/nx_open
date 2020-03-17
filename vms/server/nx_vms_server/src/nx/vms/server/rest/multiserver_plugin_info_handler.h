#pragma once

#include <core/resource/resource_fwd.h>

#include <rest/server/json_rest_handler.h>
#include <rest/helpers/request_context.h>

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/api/data/analytics_data.h>

namespace nx::vms::server::rest {

class MultiserverPluginInfoHandler: public QnJsonRestHandler, public ServerModuleAware
{
    struct HttpRequestContext
    {
        SystemError::ErrorCode osErrorCode;
        int httpStatusCode;
        nx::network::http::BufferType messageBody;
    };

    using JsonRequestContext = QnMultiserverRequestContext<JsonRestRequest>;

public:
    static const QString kPath;

public:
    MultiserverPluginInfoHandler(QnMediaServerModule* serverModule);
    virtual JsonRestResponse executeGet(const JsonRestRequest& request) override;

private:
    void loadLocalData(
        nx::vms::api::ExtendedPluginInfoList* outExtendedPluginInfoList);

    void loadRemoteDataAsync(
        nx::vms::api::ExtendedPluginInfoList* outExtendedPluginInfoList,
        JsonRequestContext* inOutRequestContext,
        const QnMediaServerResourcePtr& server);

    void onRemoteRequestCompletion(
        nx::vms::api::ExtendedPluginInfoList* outExtendedPluginInfoList,
        JsonRequestContext* inOutRequestContext,
        const QnMediaServerResourcePtr& server,
        const HttpRequestContext& httpRequestContext);
};

} // namespace nx::vms::server::rest
