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

enum class SystemAccessRole
{
    none = 0,
    owner = 1,
    maintenance,
    viewer,
    editor,
    editorWithSharing
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
        return accountEmail != rhs.accountEmail
            ? accountEmail < rhs.accountEmail
            : systemID < rhs.systemID;
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

}   //api
}   //cdb
}   //nx

#endif //NX_CDB_SYSTEM_DATA_H
