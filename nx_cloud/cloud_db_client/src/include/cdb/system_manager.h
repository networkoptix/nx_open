/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_API_SYSTEM_MANAGER_H
#define NX_CDB_API_SYSTEM_MANAGER_H

#include <functional>
#include <string>

#include "result_code.h"
#include "system_data.h"


namespace nx {
namespace cdb {
namespace api {

class SystemManager
{
public:
    virtual ~SystemManager() {}

    //!Binds system to an account associated with \a authzInfo
    /*!
        \note Required access role: account
    */
    virtual void bindSystem(
        SystemRegistrationData registrationData,
        std::function<void(ResultCode, SystemData)> completionHandler) = 0;
    //!Unbind system from account
    /*!
        This method MUST be called before binding system to another account
        \note Required access role: account (owner)
    */
    virtual void unbindSystem(
        const std::string& systemID,
        std::function<void(ResultCode)> completionHandler) = 0;
    //!Fetch all systems, allowed for current credentials
    /*!
        E.g., for system credentials, only one system is returned. 
        For account credentials systems of account are returned.
        \note Required access role: account, cloud_db module (e.g., connection_mediator)
    */
    virtual void getSystems(
        std::function<void(ResultCode, api::SystemDataList)> completionHandler ) = 0;
    //TODO #ak get systems by filter
    //!Share system with specified account. Operation allowed for system owner and editor_with_sharing only
    /*!
        \note Required access role: account (owner or editor_with_sharing)
    */
    virtual void shareSystem(
        SystemSharing sharingData,
        std::function<void(ResultCode)> completionHandler) = 0;
};

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_API_SYSTEM_MANAGER_H
