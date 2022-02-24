// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <vector>

namespace nx::network {

class NX_NETWORK_API AppInfo
{
public:
    static std::string realm();

    static std::string defaultCloudPortalUrl(const std::string& cloudHost);
    static std::string defaultCloudModulesXmlUrl(const std::string& cloudHost);
};

} // namespace nx::network
