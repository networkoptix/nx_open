#pragma once

#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_data.h>

namespace ec2 {

class AbstractPersistentStorage
{
public:
    ~AbstractPersistentStorage() {}

    virtual nx::vms::api::MediaServerData getServer(const QnUuid&) = 0;
    virtual nx::vms::api::UserData getUser(const QnUuid&) = 0;
};

} // namespace ec2
