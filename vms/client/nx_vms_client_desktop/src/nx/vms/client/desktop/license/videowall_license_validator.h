// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/license/validator.h>

namespace nx::vms::client::desktop {
namespace license {

/**
 * Special client-side Video Wall license validator that allows Video Wall to function
 * within a certain period of time after the server with Video Wall license went offline.
 */
class VideoWallLicenseValidator: public nx::vms::license::Validator
{
    using base_type = Validator;

public:
    using base_type::base_type;

    virtual bool overrideMissingRuntimeInfo(
        const QnLicensePtr& license, QnPeerRuntimeInfo& info) const override;
};

} // namespace license
} // namespace nx::vms::client::desktop
