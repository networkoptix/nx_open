#pragma once

#include <nx/utils/uuid.h>

#include <core/resource_management/manual_camera_searcher.h>
#include <nx/network/rest/handler.h>
#include <api/model/manual_camera_data.h>
#include <nx/vms/server/server_module_aware.h>

#include <memory>
#include <unordered_map>

namespace nx::vms::server {

class ManualCameraAdditionRestHandler:
    public nx::network::rest::Handler,
    public /*mixin*/ ServerModuleAware
{
public:
    ManualCameraAdditionRestHandler(QnMediaServerModule* serverModule);
    ~ManualCameraAdditionRestHandler();

protected:
    virtual nx::network::rest::Response executeGet(
        const nx::network::rest::Request& request) override;

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;

private:
    nx::network::http::StatusCode::Value searchStartAction(
        const nx::network::rest::Params& params,
        nx::network::rest::JsonResult& result,
        const QnRestConnectionProcessor* owner);

    nx::network::http::StatusCode::Value searchStatusAction(
        const nx::network::rest::Params& params,
        nx::network::rest::JsonResult& result);

    nx::network::http::StatusCode::Value searchStopAction(
        const nx::network::rest::Params& params,
        nx::network::rest::JsonResult& result);

    nx::network::http::StatusCode::Value extractSearchStartParams(
        nx::network::rest::JsonResult* const result,
        const nx::network::rest::Params& params,
        nx::utils::Url* const outUrl,
        std::optional<std::pair<nx::network::HostAddress, nx::network::HostAddress>>* const outIpRange);

    /**
     * Get status of the manual camera search process. Thread-safe.
     * @param searchProcessUuid Uuid of the process.
     * @return Status of the process.
     */
    QnManualCameraSearchProcessStatus getSearchStatus(const QnUuid& searchProcessUuid);

    /**
     * Check if there is running manual camera search process with the uuid provided.
     * @param searchProcessUuid Uuid of the process.
     * @return True if process is running, false otherwise.
     */
    bool isSearchActive(const QnUuid& searchProcessUuid);

    nx::network::http::StatusCode::Value addCameras(
        const AddManualCameraData& data,
        nx::network::rest::JsonResult& result,
        const QnRestConnectionProcessor* owner);

private:
    using QnManualCameraSearcherPtr = std::unique_ptr<QnManualCameraSearcher>;

    // Mutex that is used to synchronize access to manual camera addition progress.
    QnMutex m_searchProcessMutex;
    std::unordered_map<QnUuid, QnManualCameraSearcherPtr> m_searchProcesses;
};

} // namespace nx::vms::server
