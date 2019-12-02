#pragma once

#include <licensing/license_validator.h>

namespace nx::vms::client::desktop {
namespace license {

/**
 * Special client-side Video Wall license validator that allows Video Wall to function
 * within a certain period of time after the server with Video Wall license went offline.
 */
class VideoWallLicenseValidator: public QnLicenseValidator
{
    using base_type = QnLicenseValidator;

public:
    using base_type::base_type;

    virtual bool overrideMissingRuntimeInfo(
        const QnLicensePtr& license, QnPeerRuntimeInfo& info) const override;
};

} // namespace license
} // namespace nx::vms::client::desktop
