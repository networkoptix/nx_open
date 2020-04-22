#pragma once

#include <optional>
#include <string>
#include <exception>

#include <nx/kit/json.h>
#include <nx/sdk/i_string_map.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/utils/url.h>
#include <nx/utils/thread/cf/cfuture.h>

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

        Entry<int> sensitivity{"Vca.Sensitivity"};
    };
    std::optional<Vca> vca;

    CameraSettings();

    explicit CameraSettings(const CameraFeatures& features);

    cf::future<cf::unit> fetch(nx::utils::Url url);
    cf::future<cf::unit> store(nx::utils::Url url) const;

    void parse(const nx::sdk::IStringMap& rawSettings);
    void unparse(nx::sdk::StringMap* rawSettings) const;

    nx::kit::Json buildJsonModel() const;
};

} // namespace nx::vms_server_plugins::analytics::vivotek
