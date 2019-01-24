#pragma once

#include <nx/core/resource/resource_with_local_property_storage.h>
#include <core/resource/media_server_resource.h>

namespace nx::core::resource {

class ServerMock: public ResourceWithLocalPropertyStorage<QnMediaServerResource>
{
    using base_type = ResourceWithLocalPropertyStorage<QnMediaServerResource>;
public:

    ServerMock(QnCommonModule* commonModule): base_type(commonModule) {};
};

} // namespace nx::core::resource
