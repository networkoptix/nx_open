/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_SYSTEM_MANAGER_H
#define NX_CDB_CL_SYSTEM_MANAGER_H

#include "include/cdb/system_manager.h"


namespace nx {
namespace cdb {
namespace cl {

class SystemManager
:
    public api::SystemManager
{
public:
    //!Implementation of \a SystemManager::bindSystem
    void bindSystem(
        api::SystemRegistrationData registrationData,
        std::function<void(api::ResultCode, api::SystemData)> completionHandler) override;
    //!Implementation of \a SystemManager::unbindSystem
    void unbindSystem(
        const std::string& systemID,
        std::function<void(api::ResultCode)> completionHandler) override;
    //!Implementation of \a SystemManager::getSystems
    void getSystems(
        std::function<void(api::ResultCode, std::vector<api::SystemData>)> completionHandler ) override;
    //!Implementation of \a SystemManager::shareSystem
    void shareSystem(
        api::SystemSharing sharingData,
        std::function<void(api::ResultCode)> completionHandler) override;
};


}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_SYSTEM_MANAGER_H
