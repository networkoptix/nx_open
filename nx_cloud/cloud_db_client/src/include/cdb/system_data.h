/**********************************************************
* Sep 3, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_SYSTEM_DATA_H
#define NX_CDB_SYSTEM_DATA_H

#include <string>
#include <vector>

#include <utils/common/uuid.h>


namespace nx {
namespace cdb {
namespace api {


class SubscriptionData
{
public:
    std::string productID;
    std::string systemID;
};

namespace SystemAccessRole
{
    enum Value
    {
        none = 0,
        owner = 1,
        maintenance,
        viewer,
        editor,
        editorWithSharing
    };
}

//!Information required to register system in cloud
class SystemRegistrationData
{
public:
    //!Not unique system name
    std::string name;
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
    QnUuid id;
    //!Not unique system name
    std::string name;
    //!Key, system uses to authenticate requests to any cloud module
    std::string authKey;
    QnUuid ownerAccountID;
    SystemStatus status;
    //!a true, if cloud connection is activated for this system
    bool cloudConnectionSubscriptionStatus;

    SystemData()
    :
        status(ssInvalid),
        cloudConnectionSubscriptionStatus(false)
    {
    }
};


class SystemSharing
{
public:
    QnUuid accountID;
    QnUuid systemID;
    SystemAccessRole::Value accessRole;

    SystemSharing()
    :
        accessRole(SystemAccessRole::none)
    {
    }
};

}   //api
}   //cdb
}   //nx

#endif //NX_CDB_SYSTEM_DATA_H
