// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_data_helper.h"

namespace {

static const auto kDeviceUrlAttributeName = "deviceUrlAttribute";
static const auto kDeviceModelAttributeName = "deviceModelAttribute";

} // namespace

namespace nx::vms::common::system_health {

nx::common::metadata::Attributes storeDiscoveredDeviceDataToAttributes(
    const nx::utils::Url& deviceUrl,
    const QString& deviceModel)
{
    return {
        {kDeviceUrlAttributeName, deviceUrl.toString()},
        {kDeviceModelAttributeName, deviceModel}};
}

std::optional<nx::utils::Url> getDeviceUrlFromAttributes(
    const nx::common::metadata::Attributes& attributes)
{
    const auto itr = findFirstAttributeByName(&attributes, kDeviceUrlAttributeName);
    return itr != attributes.cend()
        ? std::make_optional(nx::utils::Url(itr->value))
        : std::nullopt;
}

std::optional<QString> getDeviceModelFromAttributes(
    const nx::common::metadata::Attributes& attributes)
{
    const auto itr = findFirstAttributeByName(&attributes, kDeviceModelAttributeName);
    return itr != attributes.cend()
        ? std::make_optional(itr->value)
        : std::nullopt;
}

} // namespace nx::vms::common::system_health
