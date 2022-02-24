// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/core/resource/resource_with_local_property_storage.h>
#include <core/resource/media_server_resource.h>

namespace nx::core::resource {

class ServerMock: public ResourceWithLocalPropertyStorage<QnMediaServerResource>
{
};

} // namespace nx::core::resource
