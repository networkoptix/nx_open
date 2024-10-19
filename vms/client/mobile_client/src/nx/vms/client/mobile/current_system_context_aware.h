// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::mobile {

/**
 * Helper class which provides access to the one and only mobile client System Context.
 */
class CurrentSystemContextAware: public core::SystemContextAware
{
public:
    CurrentSystemContextAware();
};

} // namespace nx::vms::client::mobile
