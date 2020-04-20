#pragma once

#include <exception>

#include <nx/utils/url.h>
#include <nx/utils/move_only_func.h>

namespace nx::vms_server_plugins::analytics::vivotek {

struct CameraFeatures
{
    bool vca = false;

    void fetch(nx::utils::Url url,
        nx::utils::MoveOnlyFunc<void(std::exception_ptr)> handler);
};

} // namespace nx::vms_server_plugins::analytics::vivotek
