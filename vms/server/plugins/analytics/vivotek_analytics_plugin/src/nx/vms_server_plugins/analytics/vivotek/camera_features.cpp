#include "camera_features.h"

#include <nx/utils/log/log_message.h>

#include "camera_parameter_api.h"
#include "exception.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;

CameraFeatures CameraFeatures::fetchFrom(const nx::utils::Url& cameraUrl)
{
    try
    {
        const auto parameters = CameraParameterApi(cameraUrl).fetch({"vadp_module"});

        CameraFeatures features;

        const auto count = toInt(at(parameters, "vadp_module_number"));
        for (int i = 0; i < count; ++i)
        {
            if (at(parameters, NX_FMT("vadp_module_i%1_name", i)) == "VCA")
            {
                features.vca = true;
                break;
            }
        }

        return features;
    }
    catch (Exception& exception)
    {
        exception.addContext("Failed to fetch features from camera at %1", cameraUrl);
        throw;
    }
}

} // namespace nx::vms_server_plugins::analytics::vivotek
