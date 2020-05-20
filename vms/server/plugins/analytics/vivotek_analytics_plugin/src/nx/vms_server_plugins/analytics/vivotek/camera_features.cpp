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
    CameraVcaParameterApi api(cameraUrl, "Functions");
    const auto functions = get<QJsonArray>(api.fetch().get());
    for (int i = 0; i < functions.count(); ++i)
    {
        const auto function = get<QString>("$.Functions", functions, i);
        
        vca->crowd |= function.contains("CrowdDetection");
        vca->loitering |= function.contains("LoiteringDetection");
        vca->intrusion |= function.contains("IntrusionDetection");
        vca->lineCrossing |= function.contains("LineCrossingDetection");
        vca->missingObject |= function.contains("MissingObjectDetection");
        vca->unattendedObject |= function.contains("UnattendedObjectDetection");
        vca->face |= function.contains("FaceDetection");
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
