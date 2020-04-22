#include "camera_settings.h"

#include <exception>

#include <nx/network/http/http_async_client.h>

#include "future_utils.h"
#include "parameter_api.h"
#include "http_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::utils;
using namespace nx::network;

namespace {
    template <typename Vca, typename Visitor>
    void forEachOtherVcaEntry(Vca* vca, Visitor visit)
    {
        visit(&vca->installation.height, "CamHeight");
        visit(&vca->installation.tiltAngle, "TiltAngle");
        visit(&vca->installation.rollAngle, "RollAngle");
        visit(&vca->sensitivity, "Sensitivity");
    }
    
    void parseOtherVca(CameraSettings::Entry<int>* entry, const Json& rawVca, const char* name)
    {
        const auto& value = rawVca[name];
        if (!value.is_number())
            throw std::runtime_error("$." + std::string(name) + " in VCA settings json is not a number");
        entry->value = value.int_value();
    }

    cf::future<cf::unit> fetchVcaEnabled(bool* enabled, Url url)
    {
        const auto api = std::make_shared<ParameterApi>(url);
        return api->get({"vadp_module"}).then(
            [enabled, api](auto future)
            {
                const auto parameters = future.get();

                const int count = std::stoi(parameters.at("vadp_module_number"));
                for (int i = 0; i < count; ++i)
                {
                    const auto prefix = "vadp_module_i" + std::to_string(i);
                    if (parameters.at(prefix + "_name") == "VCA")
                    {
                        const auto value = parameters.at(prefix + "_status");

                        *enabled = value != "off";
                        
                        return cf::unit();
                    }
                }

                throw std::runtime_error("Camera suddenly stopped supporting VCA");
            });
    }

    cf::future<cf::unit> fetchVca(CameraSettings::Vca* vca, Url url)
    {
        return fetchVcaEnabled(&vca->enabled.value, url).then(
            [vca, url = std::move(url)](auto future) mutable
            {
                future.get();

                if (!vca->enabled.value)
                    return cf::make_ready_future(cf::unit());

                url.setPath("/VCA/Config/AE");

                const auto httpClient = std::make_shared<http::AsyncClient>();
                return initiateFuture(
                    [httpClient, url = std::move(url)](auto promise)
                    {
                        httpClient->doGet(std::move(url),
                            [promise = std::move(promise)]() mutable
                            {
                                promise.set_value(cf::unit());
                            });
                    })
                .then(
                    [vca, httpClient](auto future)
                    {
                        future.get();

                        checkResponse(*httpClient);

                        const auto body = httpClient->fetchMessageBodyBuffer();

                        std::string error;
                        const auto rawVca = Json::parse(body.data(), error);
                        if (!error.empty())
                            throw std::runtime_error(
                                "Failed to parse VCA settings json: " + error);

                        forEachOtherVcaEntry(vca,
                            [&](auto* entry, const char* name) {
                                parseOtherVca(entry, rawVca, name);
                            });

                        vca->installation.height.value /= 10; // cm in UI, but api returns as mm

                        return cf::unit();
                    });
            });
    }

    template <typename Settings, typename Visitor>
    void forEachEntry(Settings* settings, Visitor visit)
    {
        if (settings->vca) {
            visit(&settings->vca->enabled);
            visit(&settings->vca->installation.height);
            visit(&settings->vca->installation.tiltAngle);
            visit(&settings->vca->installation.rollAngle);
            visit(&settings->vca->sensitivity);
        }
    }

    void parse(CameraSettings::Entry<bool>* entry, const IStringMap& rawSettings)
    {
        if (const char* value = rawSettings.value(entry->name.data()))
            entry->value = std::strcmp(value, "false");
    }

    void parse(CameraSettings::Entry<int>* entry, const IStringMap& rawSettings)
    {
        if (const char* value = rawSettings.value(entry->name.data()))
            entry->value = std::stoi(value);
    }

    void unparse(StringMap* rawSettings, const CameraSettings::Entry<bool>& entry)
    {
        rawSettings->setItem(entry.name, entry.value ? "true" : "false");
    }

    void unparse(StringMap* rawSettings, const CameraSettings::Entry<int>& entry)
    {
        rawSettings->setItem(entry.name, std::to_string(entry.value));
    }
} //namespace

CameraSettings::CameraSettings() = default;

CameraSettings::CameraSettings(const CameraFeatures& features)
{
    if (features.vca)
        vca.emplace();
}

cf::future<cf::unit> CameraSettings::fetch(Url url)
{
    return cf::make_ready_future(cf::unit()).then(
        [this, url = std::move(url)](auto future) mutable
        {
            future.get();

            if (!vca)
                return cf::make_ready_future(cf::unit());

            return fetchVca(&*vca, std::move(url));
        });
}

cf::future<cf::unit> CameraSettings::store(nx::utils::Url url) const
{
    return cf::make_ready_future(cf::unit());
}

void CameraSettings::parse(const IStringMap& rawSettings)
{
    forEachEntry(this,
        [&](auto* entry)
        {
            vivotek::parse(entry, rawSettings);
        });
}

void CameraSettings::unparse(StringMap* rawSettings) const
{
    forEachEntry(this,
        [&](const auto* entry)
        {
            vivotek::unparse(rawSettings, *entry);
        });
}

Json CameraSettings::buildJsonModel() const
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
                                Json::object{
                                    {"name", vca->sensitivity.name},
                                    {"type", "SpinBox"},
                                    {"caption", "Sensitivity"},
                                    {"description", "The higher the value the more likely an object will be detected as human"},
                                    {"minValue", 1},
                                    {"maxValue", 10},
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
