#include "camera_settings.h"

#include <stdexcept>

#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/utils/log/log_message.h>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "http_client.h"
#include "camera_parameter_api.h"
#include "camera_vca_parameter_api.h"
#include "json_utils.h"
#include "exception_utils.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::utils;
using namespace nx::network;

namespace {

class VadpModuleInfo
{
public:
    int index = -1;
    bool enabled = false;

public:
    static VadpModuleInfo fetchFrom(const Url& cameraUrl, const QString& moduleName)
    {
        try
        {
            const auto parameters = CameraParameterApi(cameraUrl).fetch({"vadp_module"});

            const auto count = toInt(parameters.at("vadp_module_number"));
            for (int i = 0; i < count; ++i)
            {
                if (parameters.at(NX_FMT("vadp_module_i%1_name", i)) == moduleName)
                    return {i, parseStatus(parameters.at(NX_FMT("vadp_module_i%1_status", i)))};
            }

            throw std::runtime_error("Module is not installed");
        }
        catch (const std::exception&)
        {
            rethrowWithContext("Failed to fetch info for %1 VADP module", moduleName);
        }
    }

private:
    static bool parseStatus(const QString& unparsedValue)
    {
        if (unparsedValue == "off")
            return false;
        if (unparsedValue == "on")
            return true;
        throw std::runtime_error("Failed to parse module status");
    }
};


template <typename Vca, typename Visitor>
void enumerateVcaEntries(Vca* vca, Visitor visit)
{
    // intentionaly skip vca->enabled, since it's processed separately
    visit(&vca->installation.height, "CamHeight");
    visit(&vca->installation.tiltAngle, "TiltAngle");
    visit(&vca->installation.rollAngle, "RollAngle");
    visit(&vca->sensitivity, "Sensitivity");
}


void fetchFromCamera(CameraSettings::Vca::Enabled* enabled, const Url& cameraUrl)
{
    try
    {
        const auto moduleInfo = VadpModuleInfo::fetchFrom(cameraUrl, "VCA");
        enabled->emplaceValue(moduleInfo.enabled);
    }
    catch (const std::exception& exception)
    {
        enabled->emplaceErrorMessage(collectNestedMessages(exception));
    }
}


template <typename Type>
void parseVcaFromCamera(CameraSettings::Entry<Type>* entry,
    const QJsonValue& parameters, const QString& propertyName)
{
    get(&entry->emplaceValue(), parameters, propertyName);
}

template <typename... Args>
void parseVcaFromCamera(CameraSettings::Vca::Installation::Height* entry,
    const QJsonValue& parameters, const QString& propertyName)
{
    auto& value = entry->emplaceValue();
    get(&value, parameters, propertyName);
    value /= 10; // API returns millimeters, while web UI presents centimeters
}


void fetchFromCamera(CameraSettings::Vca* vca, const Url& cameraUrl)
{
    auto& enabled = vca->enabled;
    fetchFromCamera(&enabled, cameraUrl);

    if (!enabled.hasValue() || !enabled.value())
    {
        enumerateVcaEntries(vca,
            [&](auto* entry, auto&&) { entry->emplaceNothing(); });
        return;
    }

    try
    {
        const auto parameters = CameraVcaParameterApi(cameraUrl).fetch().get();
        enumerateVcaEntries(vca,
            [&](auto* entry, const QString& propertyName)
            {
                try
                {
                    parseVcaFromCamera(entry, parameters, propertyName);
                }
                catch (const std::exception& exception)
                {
                    entry->emplaceErrorMessage(collectNestedMessages(exception));
                }
            });
    }
    catch (const std::exception& exception)
    {
        const auto message = collectNestedMessages(exception);
        enumerateVcaEntries(vca,
            [&](auto* entry, auto&&) { entry->emplaceErrorMessage(message); });
    }
}


void storeToCamera(const Url& cameraUrl, CameraSettings::Vca::Enabled* enabled)
{
    try
    {
        if (!enabled->hasValue())
            return;

        const auto moduleInfo = VadpModuleInfo::fetchFrom(cameraUrl, "VCA");
        if (moduleInfo.enabled == enabled->value())
            return;

        QUrlQuery query;
        query.addQueryItem("idx", QString::number(moduleInfo.index));
        query.addQueryItem("cmd", enabled->value() ? "start" : "stop");

        auto url = cameraUrl;
        url.setPath("/cgi-bin/admin/vadpctrl.cgi");
        url.setQuery(query);

        const auto responseBody = HttpClient().get(url).get();

        const auto successPattern =
            NX_FMT("'VCA is %1'", enabled->value() ? "started" : "stopped").toUtf8();
        if (!responseBody.contains(successPattern))
        {
            throw std::runtime_error(
                "HTTP response doesn't contain expected pattern indicating success");
        }
    }
    catch (const std::exception& exception)
    {
        enabled->emplaceErrorMessage(collectNestedMessages(exception));
    }
}


template <typename Type>
void unparseVcaToCamera(QJsonValue* parameters, const QString& propertyName,
    const CameraSettings::Entry<Type>& entry)
{
    if (!entry.hasValue())
        return;

    set(parameters, propertyName, entry.value());
}

void unparseVcaToCamera(QJsonValue* parameters, const QString& propertyName,
    const CameraSettings::Vca::Installation::Height& entry)
{
    if (!entry.hasValue())
        return;

    // API returns millimeters, while web UI presents centimeters
    set(parameters, propertyName, entry.value() * 10);
}


void storeToCamera(const Url& cameraUrl, CameraSettings::Vca* vca)
{
    auto& enabled = vca->enabled;
    storeToCamera(cameraUrl, &enabled);

    if (!enabled.hasValue() || !enabled.value())
    {
        enumerateVcaEntries(vca,
            [&](auto* entry, auto&&) { entry->emplaceNothing(); });
        return;
    }

    try
    {
        CameraVcaParameterApi api(cameraUrl);

        auto parameters = api.fetch().get();

        enumerateVcaEntries(vca,
            [&](auto* entry, const QString& propertyName)
            {
                try
                {
                    unparseVcaToCamera(&parameters, propertyName, *entry);
                }
                catch (const std::exception& exception)
                {
                    entry->emplaceErrorMessage(collectNestedMessages(exception));
                }
            });

        api.store(parameters).get();
    }
    catch (const std::exception& exception)
    {
        const auto message = collectNestedMessages(exception);
        enumerateVcaEntries(vca,
            [&](auto* entry, auto&&) { entry->emplaceErrorMessage(message); });
    }
}


template <typename Settings, typename Visitor>
void enumerateEntries(Settings* settings, Visitor visit)
{
    if (settings->vca)
    {
        visit(&settings->vca->enabled);
        visit(&settings->vca->installation.height);
        visit(&settings->vca->installation.tiltAngle);
        visit(&settings->vca->installation.rollAngle);
        visit(&settings->vca->sensitivity);
    }
}


void parseEntryValueFromServer(bool* value, const QString& unparsedValue)
{
    if (unparsedValue == "false")
        *value = false;
    else if (unparsedValue == "true")
        *value = true;
    else
        throw std::runtime_error("Failed to parse boolean");
}

void parseEntryValueFromServer(int* value, const QString& unparsedValue)
{
    *value = toInt(unparsedValue);
}


template <typename Type>
QString unparseEntryValueToServer(Type value) = delete;

QString unparseEntryValueToServer(bool value)
{
    return value ? "true" : "false";
}

QString unparseEntryValueToServer(int value)
{
    return QString::number(value);
}


QJsonValue getVcaInstallationModelForManifest()
{
    using Installation = CameraSettings::Vca::Installation;
    return QJsonObject{
        {"type", "GroupBox"},
        {"caption", "Camera installation"},
        {"items", QJsonArray{
            QJsonObject{
                {"name", Installation::Height::name},
                {"type", "SpinBox"},
                {"caption", "Height (cm)"},
                {"description", "Distance between camera and floor"},
                {"defaultValue", 200},
                {"minValue", 0},
                {"maxValue", 2000},
            },
            QJsonObject{
                {"name", Installation::TiltAngle::name},
                {"type", "SpinBox"},
                {"caption", "Tilt angle (°)"},
                {"description", "Angle between camera direction axis and down direction"},
                {"defaultValue", 0},
                {"minValue", 0},
                {"maxValue", 179},
            },
            QJsonObject{
                {"name", Installation::RollAngle::name},
                {"type", "SpinBox"},
                {"caption", "Roll angle (°)"},
                {"description", "Angle of camera rotation around direction axis"},
                {"defaultValue", 0},
                {"minValue", -74},
                {"maxValue", +74},
            },
        }},
    };
}

QJsonValue getVcaModelForManifest()
{
    using Vca = CameraSettings::Vca;
    return QJsonObject{
        {"type", "Section"},
        {"caption", "Deep Learning VCA"},
        {"items", QJsonArray{
            QJsonObject{
                {"name", Vca::Enabled::name},
                {"type", "SwitchButton"},
                {"caption", "Enabled"},
                {"defaultValue", false},
            },
            getVcaInstallationModelForManifest(),
            QJsonObject{
                {"name", Vca::Sensitivity::name},
                {"type", "SpinBox"},
                {"caption", "Sensitivity"},
                {"description", "The higher the value the more likely an object will be detected as human"},
                {"defaultValue", 5},
                {"minValue", 1},
                {"maxValue", 10},
            },
        }},
    };
}

} //namespace

CameraSettings::CameraSettings(const CameraFeatures& features)
{
    if (features.vca)
        vca.emplace();
}

void CameraSettings::fetchFrom(const Url& cameraUrl)
{
    if (vca)
        fetchFromCamera(&*vca, cameraUrl);
}

void CameraSettings::storeTo(const Url& cameraUrl)
{
    if (vca)
        storeToCamera(cameraUrl, &*vca);
}

void CameraSettings::parseFromServer(const IStringMap& values)
{
    enumerateEntries(this,
        [&](auto* entry)
        {
            try
            {
                const char* unparsedValue = values.value(entry->name);
                if (!unparsedValue)
                {
                    entry->emplaceNothing();
                    return;
                }

                parseEntryValueFromServer(&entry->emplaceValue(), unparsedValue);
            }
            catch (const std::exception& exception)
            {
                entry->emplaceErrorMessage(collectNestedMessages(exception));
            }
        });
}

Ptr<StringMap> CameraSettings::unparseToServer()
{
    auto values = makePtr<StringMap>();

    enumerateEntries(this,
        [&](auto* entry)
        {
            try
            {
                if (!entry->hasValue())
                    return;

                values->setItem(entry->name, unparseEntryValueToServer(entry->value()).toStdString());
            }
            catch (const std::exception& exception)
            {
                entry->emplaceErrorMessage(collectNestedMessages(exception));
            }
        });

    return values;
}

Ptr<StringMap> CameraSettings::getErrorMessages() const
{
    auto errorMessages = makePtr<StringMap>();

    enumerateEntries(this,
        [&](const auto* entry)
        {
            if (!entry->hasError())
                return;

            errorMessages->setItem(entry->name, entry->errorMessage().toStdString());
        });

    return errorMessages;
}

QJsonValue CameraSettings::getModelForManifest(const CameraFeatures& features)
{
    return QJsonObject{
        {"type", "Settings"},
        {"sections",
            [&]{
                QJsonArray sections;

                if (features.vca)
                    sections.push_back(getVcaModelForManifest());

                return sections;
            }(),
        },
    };
}

} // namespace nx::vms_server_plugins::analytics::vivotek
