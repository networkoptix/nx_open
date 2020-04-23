#include "camera_features.h"

#include <stdexcept>
#include <string>

#include "parameter_api.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;

cf::future<cf::unit> CameraFeatures::fetch(Url url)
{
    return cf::make_ready_future(cf::unit()).then(
        [this, url = std::move(url)](auto future) mutable
        {
            future.get();

            const auto api = std::make_shared<ParameterApi>(std::move(url));
            return api->get({"vadp_module"}).then(
                [this, api](auto future) mutable
                {
                    const auto parameters = future.get();

                    vca = false;

                    int count = std::stoi(parameters.at("vadp_module_number"));
                    for (int i = 0; i < count; ++i)
                        if (parameters.at("vadp_module_i" + std::to_string(i) + "_name") == "VCA")
                            vca = true;

                    return cf::unit();
                });
        })
    .then(
        [](auto future)
        {
            try
            {
                return future.get();
            }
            catch (...)
            {
                std::throw_with_nested(std::runtime_error(
                    "Failed to fetch camera features"));
            }
        });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
