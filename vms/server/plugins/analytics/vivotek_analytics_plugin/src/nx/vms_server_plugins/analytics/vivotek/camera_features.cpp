#include "camera_features.h"

#include <string>

#include "parameter_api.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;

namespace {
    void parse(CameraFeatures* features,
        const std::unordered_map<std::string, std::string>& parameters)
    {
        features->vca = false;

        int count = std::stoi(parameters.at("vadp_module_number"));
        for (int i = 0; i < count; ++i)
            if (parameters.at("vadp_module_i" + std::to_string(i) + "_name") == "VCA")
                features->vca = true;
    }
} // namespace

void CameraFeatures::fetch(Url url, MoveOnlyFunc<void(std::exception_ptr)> handler)
{
    auto api = std::make_shared<ParameterApi>(std::move(url));
    api->get({"vadp_module"},
        [this, api, handler = std::move(handler)](auto exceptionPtr, auto parameters) mutable
        {
            try
            {
                if (exceptionPtr)
                    std::rethrow_exception(exceptionPtr);

                parse(this, parameters);

                handler(nullptr);
            }
            catch (...)
            {
                handler(std::current_exception());
            }
        });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
