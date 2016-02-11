/**********************************************************
* Sep 3, 2015
* NetworkOptix
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

    /** Binds system to an account associated with \a authzInfo
        \note Required access role: account
    */
    virtual void bindSystem(
        SystemRegistrationData registrationData,
        std::function<void(ResultCode, SystemData)> completionHandler) = 0;
    /** Unbind system from account
        This method MUST be called before binding system to another account
        \note Required access role: account (owner)
    */
    virtual void unbindSystem(
        const std::string& systemID,
        std::function<void(ResultCode)> completionHandler) = 0;
    /** Fetch all systems, allowed for current credentials
        E.g., for system credentials, only one system is returned. 
        For account credentials systems of account are returned.
        \TODO #ak add filter
        \note Required access role: account, cloud_db module (e.g., connection_mediator)
    */
    virtual void getSystems(
        std::function<void(ResultCode, api::SystemDataExList)> completionHandler ) = 0;
    /** Get system by id */
    virtual void getSystem(
        const std::string& systemID,
        std::function<void(ResultCode, api::SystemDataExList)> completionHandler) = 0;
    /** Share system with specified account. Operation allowed for system owner and editor_with_sharing only
        \note Required access role: account (owner or editor_with_sharing)
        \note sharing is removed if \a sharingData.accessRole is \a api::SystemAccessRole::none
    */
    virtual void shareSystem(
        SystemSharing sharingData,
        std::function<void(ResultCode)> completionHandler) = 0;
    /** Returns sharings (account email, access role) for every system of account authorized */
    virtual void getCloudUsersOfSystem(
        std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler) = 0;
    /** Returns sharings (account email, access role) for the specified system
        \note \a owner or \a cloudAdmin account credentials MUST be provided
    */
    virtual void getCloudUsersOfSystem(
        const std::string& systemID,
        std::function<void(api::ResultCode, api::SystemSharingExList)> completionHandler) = 0;
    /** Returns list of access roles which can be used to share system \a systemID.
        \note request is authorized with account credentials
    */
    virtual void getAccessRoleList(
        const std::string& systemID,
        std::function<void(api::ResultCode, api::SystemAccessRoleList)> completionHandler) = 0;
};

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_API_SYSTEM_MANAGER_H
