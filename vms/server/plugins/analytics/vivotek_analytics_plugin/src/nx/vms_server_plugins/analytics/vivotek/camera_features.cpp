#include "camera_features.h"

#include <nx/utils/log/log_message.h>

#include "camera_vca_parameter_api.h"
#include "camera_parameter_api.h"
#include "exception.h"
#include "json_utils.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::utils;

namespace {

void fetchFromCamera(CameraFeatures::Vca* vca, const Url& cameraUrl)
{
    CameraVcaParameterApi api(cameraUrl);

    // TODO: This is not allowed when VCA is disabled. Should probably
    // merge features into settings when settings model supports schema updates.
    const auto functions = get<QJsonArray>(api.fetch("Functions").get());

    for (int i = 0; i < functions.count(); ++i)
    {
        const auto function = get<QString>("$.Functions", functions, i);
        
        vca->crowdDetection |= function.contains("CrowdDetection");
        vca->loiteringDetection |= function.contains("LoiteringDetection");
        vca->intrusionDetection |= function.contains("IntrusionDetection");
        vca->lineCrossingDetection |= function.contains("LineCrossingDetection");
        vca->missingObjectDetection |= function.contains("MissingObjectDetection");
        vca->unattendedObjectDetection |= function.contains("UnattendedObjectDetection");
        vca->faceDetection |= function.contains("FaceDetection");
        vca->runningDetection |= function.contains("RunningDetection");
    }
}

} // namespace

CameraFeatures CameraFeatures::fetchFrom(const Url& cameraUrl)
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
                fetchFromCamera(&features.vca.emplace(), cameraUrl);
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
