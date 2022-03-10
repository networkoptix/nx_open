// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/instrument.h>

namespace nx::vms::client::core {

NX_REFLECTION_ENUM_CLASS(OauthClientType,
    undefined,
    connect,
    renewDesktop,
    loginCloud,
    passwordDisconnect,
    passwordBackup,
    passwordRestore,
    passwordMerge,
    system2faAuth)

NX_REFLECTION_ENUM_CLASS(OauthViewType,
    desktop,
    mobile)

} // namespace nx::vms::client::core
