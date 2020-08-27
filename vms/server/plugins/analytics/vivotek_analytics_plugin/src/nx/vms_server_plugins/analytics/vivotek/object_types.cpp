#include "object_types.h"

namespace nx::vms_server_plugins::analytics::vivotek {

const std::vector<ObjectType> kObjectTypes =
    []()
    {
        std::vector<ObjectType> types;
        {
            auto& type = types.emplace_back();
            type.nativeId = "Human";
            type.id = "nx.vivotek.Human";
            type.prettyName = "Human";
            type.isAvailable =
                [](const auto& settings)
                {
                    return !!settings.vca;
                };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "Face";
            type.id = "nx.vivotek.Face";
            type.prettyName = "Face";
            type.isAvailable =
                [](const auto& settings)
                {
                    auto& vca = settings.vca;
                    return vca && vca->faceDetection;
                };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "Other";
            type.nativeBehavior = "Missing";
            type.id = "nx.vivotek.MissingObject";
            type.prettyName = "Missing object";
            type.isAvailable =
                [](const auto& settings)
                {
                    auto& vca = settings.vca;
                    return vca && vca->missingObjectDetection;
                };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "Other";
            type.nativeBehavior = "Unattended";
            type.id = "nx.vivotek.UnattendedObject";
            type.prettyName = "Unattended object";
            type.isAvailable =
                [](const auto& settings)
                {
                    auto& vca = settings.vca;
                    return vca && vca->missingObjectDetection;
                };
        }
        return types;
    }();

} // namespace nx::vms_server_plugins::analytics::vivotek
