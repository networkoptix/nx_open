// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/core/skin/font_config.h>

namespace nx::vms::client::desktop {

/**
 * Facade class for convenient access to common fonts.
 */
class NX_VMS_CLIENT_DESKTOP_API FontConfig: public core::FontConfig
{
public:
    using core::FontConfig::FontConfig;

    QFont small() const;
    QFont normal() const;
    QFont large() const;
    QFont xLarge() const;
};

NX_VMS_CLIENT_DESKTOP_API FontConfig* fontConfig();

} // namespace nx::vms::client::desktop
