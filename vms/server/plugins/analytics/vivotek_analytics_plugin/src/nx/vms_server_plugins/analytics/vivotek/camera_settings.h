#pragma once

#include <optional>
#include <string>
#include <exception>

#include <nx/kit/json.h>
#include <nx/sdk/i_string_map.h>
#include <nx/utils/url.h>
#include <nx/utils/move_only_func.h>

#include "camera_features.h"

namespace nx::vms_server_plugins::analytics::vivotek {

struct CameraSettings
{
    template <typename Type>
    struct Entry
    {
        std::string const name;
        Type value = {};
    };

    struct Vca
    {
        Entry<bool> enabled{"Vca.Enabled"};

        struct Installation
        {
            Entry<int> height{"Vca.Installation.Height"};
            Entry<int> tiltAngle{"Vca.Installation.TiltAngle"};
            Entry<int> rollAngle{"Vca.Installation.RollAngle"};
        } installation;
    };
    std::optional<Vca> vca;

    CameraSettings();

    explicit CameraSettings(const CameraFeatures& features);

    void fetch(nx::utils::Url url,
        nx::utils::MoveOnlyFunc<void(std::exception_ptr)> handler);

    void store(nx::utils::Url url,
        nx::utils::MoveOnlyFunc<void(std::exception_ptr)> handler) const;

    void parse(nx::sdk::IStringMap const& rawSettings);
    void unparse(nx::sdk::IStringMap* rawSettings) const;

    nx::kit::Json buildModel() const;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
