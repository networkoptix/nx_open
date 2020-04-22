#pragma once

#include <exception>

#include <nx/utils/url.h>
#include <nx/utils/thread/cf/cfuture.h>

namespace nx::vms_server_plugins::analytics::vivotek {

struct CameraFeatures
{
    bool vca = false;

    cf::future<cf::unit> fetch(nx::utils::Url url);
};

} // namespace nx::vms_server_plugins::analytics::vivotek
