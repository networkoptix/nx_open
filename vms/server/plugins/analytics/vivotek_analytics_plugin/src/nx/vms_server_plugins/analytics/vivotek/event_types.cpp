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
            type.prettyName = "Crowd";
            type.isProlonged = true;
            type.isSupported =
                [](auto& features) { return features.vca && features.vca->crowdDetection; };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "LoiteringDetection";
            type.id = "nx.vivotek.Loitering";
            type.prettyName = "Loitering";
            type.isProlonged = true;
            type.isSupported =
                [](auto& features) { return features.vca && features.vca->loiteringDetection; };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "IntrusionDetection";
            type.id = "nx.vivotek.Intrusion";
            type.prettyName = "Intrusion";
            type.isSupported =
                [](auto& features) { return features.vca && features.vca->intrusionDetection; };
        }
        {
            auto& type = types.emplace_back();
            type.nativeId = "LineCrossingDetection";
            type.id = "nx.vivotek.LineCrossing";
            type.prettyName = "Line Crossing";
            type.isSupported =
                [](auto& features) { return features.vca && features.vca->lineCrossingDetection; };
        }
        return types;
    }();

} // namespace nx::vms_server_plugins::analytics::vivotek
