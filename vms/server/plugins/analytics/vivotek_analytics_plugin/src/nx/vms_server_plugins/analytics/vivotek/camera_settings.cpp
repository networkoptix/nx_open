#include "camera_settings.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::utils;

CameraSettings::CameraSettings() = default;

CameraSettings::CameraSettings(const CameraFeatures& features)
{
    if (features.vca)
        vca.emplace();
}

void CameraSettings::fetch(Url url, MoveOnlyFunc<void(std::exception_ptr)> handler)
{
    handler(nullptr);
}

void CameraSettings::store(nx::utils::Url url, MoveOnlyFunc<void(std::exception_ptr)> handler) const
{
    handler(nullptr);
}

void CameraSettings::parse(IStringMap const& rawSettings)
{
}

void CameraSettings::unparse(IStringMap* rawSettings) const
{
}

Json CameraSettings::buildModel() const
{
    return Json::object{
        {"type", "Settings"},
        {"sections", [&]{
            Json::array sections;

            if (vca)
                sections.push_back(Json::object{
                    {"type", "Section"},
                    {"caption", "Deep Learning VCA"},
                    {"items", Json::array{
                        Json::object{
                            {"name", vca->enabled.name},
                            {"type", "SwitchButton"},
                            {"caption", "Enabled"},
                            {"defaultValue", false},
                        },
                        Json::object{
                            {"type", "GroupBox"},
                            {"caption", "Camera installation"},
                            {"items", Json::array{
                                Json::object{
                                    {"name", vca->installation.height.name},
                                    {"type", "SpinBox"},
                                    {"caption", "Height (cm)"},
                                    {"description", "Distance between camera and floor"},
                                    {"minValue", 0},
                                    {"maxValue", 2000},
                                },
                                Json::object{
                                    {"name", vca->installation.tiltAngle.name},
                                    {"type", "SpinBox"},
                                    {"caption", "Tilt angle (°)"},
                                    {"description", "Angle between camera direction axis and down direction"},
                                    {"minValue", 0},
                                    {"maxValue", 179},
                                },
                                Json::object{
                                    {"name", vca->installation.rollAngle.name},
                                    {"type", "SpinBox"},
                                    {"caption", "Roll angle (°)"},
                                    {"description", "Angle of camera rotation around direction axis"},
                                    {"minValue", -74},
                                    {"maxValue", +74},
                                },
                            }},
                        },
                    }},
                });

            return sections;
        }()},
    };
}

} // namespace nx::vms_server_plugins::analytics::vivotek
