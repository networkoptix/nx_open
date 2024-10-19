// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/system_context_aware.h>

namespace nx::vms::client::mobile {

class SessionManager;
class SystemContext;

class SystemContextAware: public core::SystemContextAware
{
    using base_type = core::SystemContextAware;

public:
    using base_type::base_type;

    SystemContext* systemContext() const;

    SessionManager* sessionManager() const;
};

} // namespace nx::vms::client::mobile
