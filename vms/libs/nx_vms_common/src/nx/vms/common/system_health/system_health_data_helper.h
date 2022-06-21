// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <analytics/common/object_metadata.h>
#include <nx/utils/url.h>

namespace nx::vms::common::system_health {

/**
 * Stores discovered device URL and model name to the attributes. Used to transfer additional data
 * with <tt>replacedDeviceDiscovered</tt> system health notification.
 */
NX_VMS_COMMON_API nx::common::metadata::Attributes storeDiscoveredDeviceDataToAttributes(
    const nx::utils::Url& deviceUrl,
    const QString& deviceModel);

/**
 * @return Device URL retrieved from the attributes if they contain it, std::nullopt otherwise.
 */
NX_VMS_COMMON_API std::optional<nx::utils::Url> getDeviceUrlFromAttributes(
    const nx::common::metadata::Attributes& attributes);

/**
 * @return Device model name retrieved from the attributes if they contain it, std::nullopt
 *     otherwise.
 */
NX_VMS_COMMON_API std::optional<QString> getDeviceModelFromAttributes(
    const nx::common::metadata::Attributes& attributes);

} // namespace nx::vms::common::system_health
