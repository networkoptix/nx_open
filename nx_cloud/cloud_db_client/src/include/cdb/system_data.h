/**********************************************************
* Sep 3, 2015
* NetworkOptix
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_SYSTEM_DATA_H
#define NX_CDB_SYSTEM_DATA_H

#include <string>
#include <vector>

#include <nx/utils/uuid.h>


namespace nx {
namespace cdb {
namespace api {

class SubscriptionData
{
public:
    std::string productID;
    std::string systemID;
};

//!Information required to register system in cloud
class SystemRegistrationData
{
public:
    //!Not unique system name
    std::string name;
    std::string customization;
};


enum SystemStatus
{
    ssInvalid = 0,
    //!System has been bound but not a single request from that system has been received by cloud
    ssNotActivated = 1,
    ssActivated = 2
};

class SystemData
{
public:
    //!Globally unique ID of system assigned by cloud
    QnUuid id;
    //!Not unique system name
    std::string name;
    std::string customization;
    //!Key, system uses to authenticate requests to any cloud module
    std::string authKey;
    std::string ownerAccountEmail;
    SystemStatus status;
    //!a true, if cloud connection is activated for this system
    bool cloudConnectionSubscriptionStatus;

    SystemData()
    :
        status(ssInvalid),
        cloudConnectionSubscriptionStatus(true)
    {
    }

    bool operator==(const SystemData& right) const
    {
        return
            id == right.id &&
            name == right.name &&
            customization == right.customization &&
            authKey == right.authKey &&
            ownerAccountEmail == right.ownerAccountEmail &&
            status == right.status &&
            cloudConnectionSubscriptionStatus == right.cloudConnectionSubscriptionStatus;
    }
};

class SystemDataList
{
public:
    std::vector<SystemData> systems;
};


////////////////////////////////////////////////////////////
//// system sharing data
////////////////////////////////////////////////////////////

enum class SystemAccessRole
{
    none = 0,
    liveViewer = 1,
    viewer = 2,
    advancedViewer = 3,
    localAdmin = 4,
    cloudAdmin = 5,
    maintenance = 6,
    owner = 7,
};

class SystemSharing
{
public:
    std::string accountEmail;
    QnUuid systemID;
    SystemAccessRole accessRole;

    SystemSharing()
    :
        accessRole(SystemAccessRole::none)
    {
    }

    bool operator<(const SystemSharing& rhs) const
    {
        if (accountEmail != rhs.accountEmail)
            return accountEmail < rhs.accountEmail;
        return systemID < rhs.systemID;
    }
    bool operator==(const SystemSharing& rhs) const
    {
        return accountEmail == rhs.accountEmail
            && systemID == rhs.systemID
            && accessRole == rhs.accessRole;
    }
};

class SystemSharingList
{
public:
    std::vector<SystemSharing> sharing;
};

/** adds account's full name to \a SystemSharing */
class SystemSharingEx
:
    public SystemSharing
{
public:
    SystemSharingEx() {}

    std::string fullName;

    bool operator==(const SystemSharingEx& rhs) const
    {
        return static_cast<const SystemSharing&>(*this) == static_cast<const SystemSharing&>(rhs)
            && fullName == rhs.fullName;
    }
};

class SystemSharingExList
{
public:
    std::vector<SystemSharingEx> sharing;
};

class SystemAccessRoleData
{
public:
    SystemAccessRole accessRole;

    SystemAccessRoleData()
    :
        accessRole(SystemAccessRole::none)
    {
    }

    SystemAccessRoleData(SystemAccessRole _accessRole)
    :
        accessRole(_accessRole)
    {
    }
};

class SystemAccessRoleList
{
public:
    std::vector<SystemAccessRoleData> accessRoles;
};

class SystemDataEx
:
    public SystemData
{
public:
    std::string ownerFullName;
    SystemAccessRole accessRole;
    /** permissions, account can share current system with */
    std::vector<SystemAccessRoleData> sharingPermissions;

    SystemDataEx()
    :
        accessRole(SystemAccessRole::none)
    {
    }

    SystemDataEx(SystemData systemData)
    :
        SystemData(std::move(systemData)),
        accessRole(SystemAccessRole::none)
    {
    }
};

class SystemDataExList
{
public:
    std::vector<SystemDataEx> systems;
};

}   //api
}   //cdb
}   //nx

#endif //NX_CDB_SYSTEM_DATA_H
