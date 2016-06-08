/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_SYSTEM_MANAGER_H
#define NX_CDB_CL_SYSTEM_MANAGER_H

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "include/cdb/system_manager.h"


namespace nx {
namespace cdb {
namespace cl {

class SystemManager
:
    public api::SystemManager,
    public AsyncRequestsExecutor
{
public:
    SystemManager(network::cloud::CloudModuleEndPointFetcher* const cloudModuleEndPointFetcher);

    virtual void bindSystem(
        api::SystemRegistrationData registrationData,
        std::function<void(api::ResultCode, api::SystemData)> completionHandler) override;
    virtual void unbindSystem(
        const std::string& systemID,
        std::function<void(api::ResultCode)> completionHandler) override;
    virtual void getSystems(
        std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler ) override;
    virtual void getSystem(
        const std::string& systemID,
        std::function<void(api::ResultCode, api::SystemDataExList)> completionHandler) override;
    virtual void shareSystem(
        api::SystemSharing sharingData,
        std::function<void(api::ResultCode)> completionHandler) override;
    virtual void getCloudUsersOfSystem(
        std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler) override;
    virtual void getCloudUsersOfSystem(
        const std::string& systemID,
        std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler) override;
    virtual void getAccessRoleList(
        const std::string& systemID,
        std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler) override;
    virtual void SystemManager::updateSystemName(
        const std::string& systemID,
        const std::string& systemName,
        std::function<void(api::ResultCode)> completionHandler) override;
};

}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_SYSTEM_MANAGER_H
