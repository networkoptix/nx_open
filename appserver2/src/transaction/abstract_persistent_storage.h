#pragma once

#include <nx_ec/data/api_media_server_data.h>
#include <nx_ec/data/api_user_data.h>

namespace ec2 {

class AbstractPersistentStorage
{
public:
    ~AbstractPersistentStorage() {}

    virtual ec2::ApiMediaServerData getServer(const QnUuid&) = 0;
    virtual ec2::ApiUserData getUser(const QnUuid&) = 0;
};

} // namespace ec2
