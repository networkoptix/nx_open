#pragma once

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "include/nx/cloud/cdb/api/system_manager.h"

namespace nx {
namespace cdb {
namespace client {

class SystemManager:
    public api::SystemManager,
    public AsyncRequestsExecutor
{
public:
    SystemManager(network::cloud::CloudModuleUrlFetcher* const cloudModuleEndPointFetcher);

    virtual void bindSystem(
        api::SystemRegistrationData registrationData,
        std::function<void(api::ResultCode, api::SystemData)> completionHandler) override;
    virtual void unbindSystem(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) override;
    virtual void getSystems(
        std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler ) override;
    virtual void getSystemsFiltered(
        const api::Filter& filter,
        std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler) override;
    virtual void getSystem(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler) override;
    virtual void shareSystem(
        api::SystemSharing sharingData,
        std::function<void(api::ResultCode)> completionHandler) override;
    virtual void getCloudUsersOfSystem(
        std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler) override;
    virtual void getCloudUsersOfSystem(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler) override;
    virtual void getAccessRoleList(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler) override;
    virtual void update(
        const api::SystemAttributesUpdate& updatedData,
        std::function<void(api::ResultCode)> completionHandler) override;
    virtual void rename(
        const std::string& systemId,
        const std::string& systemName,
        std::function<void(api::ResultCode)> completionHandler) override;
    virtual void recordUserSessionStart(
        const std::string& systemId,
        std::function<void(api::ResultCode)> completionHandler) override;
    virtual void getSystemHealthHistory(
        const std::string& systemId,
        std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler) override;
    virtual void startMerge(
        const std::string& idOfSystemToMergeTo,
        const std::string& idOfSystemBeingMerged,
        std::function<void(api::ResultCode)> completionHandler) override;
};

} // namespace client
} // namespace cdb
} // namespace nx
