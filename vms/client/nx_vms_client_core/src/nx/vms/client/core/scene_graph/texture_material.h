// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "private/texture_ownership_decorator_p.h"

namespace nx::vms::client::core::sg {

class NX_VMS_CLIENT_CORE_API OpaqueTextureMaterial:
    public TextureOwnershipDecorator<QSGOpaqueTextureMaterial>
{
};

class NX_VMS_CLIENT_CORE_API TextureMaterial:
    public TextureOwnershipDecorator<QSGTextureMaterial>
{
};

} // namespace nx::vms::client::core::sg
