// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_health_data_helper.h"

namespace {

static const auto kDeviceUrlAttributeName = "deviceUrlAttribute";
static const auto kDeviceModelAttributeName = "deviceModelAttribute";

} // namespace

namespace nx::vms::common::system_health {

std::map<std::string, std::string> storeDiscoveredDeviceDataToAttributes(
    const nx::utils::Url& deviceUrl,
    const QString& deviceModel)
{
    return {
        {kDeviceUrlAttributeName, deviceUrl.toStdString()},
        {kDeviceModelAttributeName, deviceModel.toStdString()}};
}

std::optional<nx::utils::Url> getDeviceUrlFromAttributes(
    const std::map<std::string, std::string>& attributes)
{
    const auto itr = attributes.find(kDeviceUrlAttributeName);
    return itr != attributes.cend()
        ? std::make_optional(nx::utils::Url(itr->second))
        : std::nullopt;
}

std::optional<QString> getDeviceModelFromAttributes(
    const std::map<std::string, std::string>& attributes)
{
    const auto itr = attributes.find(kDeviceModelAttributeName);
    return itr != attributes.cend()
        ? std::make_optional(QString::fromStdString(itr->second))
        : std::nullopt;
}

} // namespace nx::vms::common::system_health
