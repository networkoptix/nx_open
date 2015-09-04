/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "system_manager.h"


namespace nx {
namespace cdb {
namespace cl {

void SystemManager::bindSystem(
    api::SystemRegistrationData registrationData,
    std::function<void(api::ResultCode, api::SystemData)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::notImplemented, api::SystemData());
}

void SystemManager::unbindSystem(
    const std::string& systemID,
    std::function<void(api::ResultCode)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::notImplemented);
}

void SystemManager::getSystems(
    std::function<void(api::ResultCode, std::vector<api::SystemData>)> completionHandler)
{
    //TODO #ak
    completionHandler(
        api::ResultCode::notImplemented,
        std::vector<api::SystemData>());
}

void SystemManager::shareSystem(
    api::SystemSharing sharingData,
    std::function<void(api::ResultCode)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::notImplemented);
}

}   //cl
}   //cdb
}   //nx
