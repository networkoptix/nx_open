// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <nx/utils/url.h>

namespace nx::vms::common::system_health {

/**
 * Stores discovered device URL and model name to the attributes. Used to transfer additional data
 * with <tt>replacedDeviceDiscovered</tt> system health notification.
 */
NX_VMS_COMMON_API std::map<std::string, std::string> storeDiscoveredDeviceDataToAttributes(
    const nx::Url& deviceUrl,
    const QString& deviceModel);

/**
 * @return Device URL retrieved from the attributes if they contain it, std::nullopt otherwise.
 */
NX_VMS_COMMON_API std::optional<nx::Url> getDeviceUrlFromAttributes(
    const std::map<std::string, std::string>& attributes);

/**
 * @return Device model name retrieved from the attributes if they contain it, std::nullopt
 *     otherwise.
 */
NX_VMS_COMMON_API std::optional<QString> getDeviceModelFromAttributes(
    const std::map<std::string, std::string>& attributes);

} // namespace nx::vms::common::system_health
