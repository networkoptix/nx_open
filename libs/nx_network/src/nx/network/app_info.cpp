// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "app_info.h"

#include <nx/utils/string.h>

#include <nx/branding.h>

namespace nx::network {

std::string AppInfo::realm()
{
    return "VMS";
}

std::string AppInfo::defaultCloudPortalUrl(const std::string& cloudHost)
{
    return nx::utils::buildString("https://", cloudHost);
}

std::string AppInfo::defaultCloudModulesXmlUrl(const std::string& cloudHost)
{
    return nx::utils::buildString("https://", cloudHost, "/discovery/v2/cloud_modules.xml");
}

} // namespace nx::network
