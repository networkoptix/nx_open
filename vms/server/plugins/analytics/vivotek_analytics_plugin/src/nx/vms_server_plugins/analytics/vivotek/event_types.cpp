#include "event_types.h"

namespace nx::vms_server_plugins::analytics::vivotek {

const std::vector<EventType> kEventTypes =
    []()
    {
        std::vector<EventType> types;
        {
            auto& type = types.emplace_back();
            type.nativeId = "CrowdDetection";
            type.id = "nx.vivotek.Crowd";
            type.prettyName = "Crowd detection";
            type.isProlonged = true;
            type.isAvailable =
                [](const auto& settings)
                {
                    const auto& vca = settings.vca;
                    return vca && vca->crowdDetection;
                };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "LoiteringDetection";
            type.id = "nx.vivotek.Loitering";
            type.prettyName = "Loitering detection";
            type.isProlonged = true;
            type.isAvailable =
                [](const auto& settings)
                {
                    const auto& vca = settings.vca;
                    return vca && vca->loiteringDetection;
                };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "IntrusionDetection";
            type.id = "nx.vivotek.Intrusion";
            type.prettyName = "Intrusion detection";
            type.isAvailable =
                [](const auto& settings)
                {
                    const auto& vca = settings.vca;
                    return vca && vca->intrusionDetection;
                };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "LineCrossingDetection";
            type.id = "nx.vivotek.LineCrossing";
            type.prettyName = "Line crossing";
            type.isAvailable =
                [](const auto& settings)
                {
                    const auto& vca = settings.vca;
                    return vca && vca->lineCrossingDetection;
                };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "MissingObjectDetection";
            type.id = "nx.vivotek.MissingObject";
            type.prettyName = "Missing object";
            type.isProlonged = true;
            type.isAvailable =
                [](const auto& settings)
                {
                    const auto& vca = settings.vca;
                    return vca && vca->missingObjectDetection;
                };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "UnattendedObjectDetection";
            type.id = "nx.vivotek.UnattendedObject";
            type.prettyName = "Unattended object";
            type.isProlonged = true;
            type.isAvailable =
                [](const auto& settings)
                {
                    const auto& vca = settings.vca;
                    return vca && vca->unattendedObjectDetection;
                };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "FaceDetection";
            type.id = "nx.vivotek.Face";
            type.prettyName = "Face detection";
            type.isProlonged = true;
            type.isAvailable =
                [](const auto& settings)
                {
                    const auto& vca = settings.vca;
                    return vca && vca->faceDetection;
                };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "RunningDetection";
            type.id = "nx.vivotek.Running";
            type.prettyName = "Running detection";
            type.isProlonged = true;
            type.isAvailable =
                [](const auto& settings)
                {
                    const auto& vca = settings.vca;
                    return vca && vca->runningDetection;
                };
        }
        types.shrink_to_fit();
        return types;
    }();

} // namespace nx::vms_server_plugins::analytics::vivotek
