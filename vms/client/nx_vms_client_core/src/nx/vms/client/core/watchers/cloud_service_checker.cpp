// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_service_checker.h"

#include <nx/reflect/from_string.h>
#include <nx/utils/std_string_utils.h>
#include <nx/vms/client/core/ini.h>

namespace nx::vms::client::core {

CloudService fromString(const std::string_view& string)
{
    return nx::reflect::fromString<CloudService>(string, CloudService::none);
}

struct CloudServiceChecker::Private
{
    Private()
    {
        const std::string_view serviceString(ini().unsupportedCloudServices);

        nx::utils::split(
            serviceString,
            nx::utils::separator::isAnyOf(" ,;"),
            [this](const std::string_view& serviceName)
            {
                if (const auto service = fromString(serviceName); service != CloudService::none)
                    unsupportedCloudServices.setFlag(service);
            },
            nx::utils::GroupToken::none,
            nx::utils::SplitterFlag::skipEmpty);
    }

    CloudServiceChecker::CloudServices unsupportedCloudServices;
};

CloudServiceChecker::CloudServiceChecker(QObject* parent): QObject(parent), d(new Private())
{
}

CloudServiceChecker::~CloudServiceChecker()
{
}

bool CloudServiceChecker::hasService(const CloudService& service) const
{
    return !d->unsupportedCloudServices.testFlag(service);
}

} // namespace nx::vms::client::core
