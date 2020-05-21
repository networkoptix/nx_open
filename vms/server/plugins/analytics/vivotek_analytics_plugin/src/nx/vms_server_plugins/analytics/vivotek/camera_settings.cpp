#include "camera_settings.h"

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
#include "exception.h"
#include "utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::utils;
using namespace nx::network;

namespace {

const auto kMaxDetectionRuleCount = 5u; // Defined in user manual.

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

            const auto count = toInt(at(parameters, "vadp_module_number"));
            for (int i = 0; i < count; ++i)
            {
                if (at(parameters, NX_FMT("vadp_module_i%1_name", i)) == moduleName)
                    return {i, parseStatus(at(parameters, NX_FMT("vadp_module_i%1_status", i)))};
            }

            throw Exception("Module is not installed");
        }
        catch (Exception& exception)
        {
            exception.addContext("Failed to fetch info for %1 VADP module", moduleName);
            throw;
        }
    }

private:
    static bool parseStatus(const QString& unparsedValue)
    {
        if (unparsedValue == "off")
            return false;
        if (unparsedValue == "on")
            return true;
        throw Exception("Failed to parse module status");
    }
};


template <typename Vca, typename Visitor>
void enumerateVcaEntries(Vca* vca, Visitor visit)
{
    // intentionaly skip vca->enabled, since it's processed separately
    visit(&vca->sensitivity, "Sensitivity");
    visit(&vca->installation.height, "CamHeight");
    visit(&vca->installation.tiltAngle, "TiltAngle");
    visit(&vca->installation.rollAngle, "RollAngle");
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
        enabled->emplaceErrorMessage(exception.what());
    }
}


template <typename Entry>
void parseVcaEntryFromCamera(Entry* entry,
    const QJsonValue& parameters, const QString& propertyName)
{
    auto& value = entry->emplaceValue();
    get(&value, parameters, propertyName);

    using Type = typename Entry::Type;
    if constexpr (std::is_same_v<Type, int>)
    {
        if (!std::strcmp(Entry::name, CameraSettings::Vca::Installation::Height::name))
            value /= 10; // API uses millimeters, while web UI uses centimeters
    }
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
        CameraVcaParameterApi api(cameraUrl, "Config/AE");

        const auto parameters = api.fetch().get();

        enumerateVcaEntries(vca,
            [&](auto* entry, const QString& propertyName)
            {
                try
                {
                    parseVcaEntryFromCamera(entry, parameters, propertyName);
                }
                catch (const std::exception& exception)
                {
                    entry->emplaceErrorMessage(exception.what());
                }
            });
    }
    catch (const std::exception& exception)
    {
        const QString message = exception.what();
        enumerateVcaEntries(vca,
            [&](auto* entry, auto&&) { entry->emplaceErrorMessage(message); });
    }
}


QByteArray getRaw(const Url& url)
{
    HttpClient httpClient;

    std::promise<std::unique_ptr<AbstractStreamSocket>> promise;
    httpClient.setOnDone(
        [&]
        {
            NX_ASSERT(httpClient.failed());
            promise.set_exception(std::make_exception_ptr(
                std::system_error(httpClient.lastSysErrorCode(), std::system_category())));
        });
    httpClient.setOnRequestHasBeenSent(
        [&](auto&&)
        {
            promise.set_value(httpClient.takeSocket());
        });
    httpClient.doGet(url);
    const auto socket = promise.get_future().get();

    socket->setNonBlockingMode(false);

    constexpr static auto kReceiveTimeout = 10s;
    socket->setRecvTimeout(kReceiveTimeout);

    constexpr static auto kMaxResponseSize = 16 * 1024;
    QByteArray response(kMaxResponseSize, Qt::Uninitialized);
    for (int offset = 0;; )
    {
        int addedLength = socket->recv(response.data() + offset, response.length() - offset);
        if (addedLength == -1 || addedLength == 0)
        {
            response.chop(offset);
            return response;
        }

        offset += addedLength;
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

        // work around camera not inserting \r\n between response headers and body
        const auto response = getRaw(url);

        const auto successPattern =
            NX_FMT("'VCA is %1'", enabled->value() ? "started" : "stopped").toUtf8();
        if (!response.contains(successPattern))
        {
            throw Exception(
                "HTTP response doesn't contain expected pattern indicating success");
        }
    }
    catch (const std::exception& exception)
    {
        enabled->emplaceErrorMessage(exception.what());
    }
}


template <typename Entry>
void unparseVcaEntryToCamera(QJsonValue* parameters, const QString& propertyName,
    const Entry& entry)
{
    if (!entry.hasValue())
        return;

    auto value = entry.value();

    using Type = typename Entry::Type;
    if constexpr (std::is_same_v<Type, int>)
    {
        if (!std::strcmp(Entry::name, CameraSettings::Vca::Installation::Height::name))
            value *= 10; // API uses millimeters, while web UI uses centimeters
    }

    set(parameters, propertyName, value);
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
        CameraVcaParameterApi api(cameraUrl, "Config/AE");

        auto parameters = api.fetch().get();

        enumerateVcaEntries(vca,
            [&](auto* entry, const QString& propertyName)
            {
                try
                {
                    unparseVcaEntryToCamera(&parameters, propertyName, *entry);
                }
                catch (const std::exception& exception)
                {
                    entry->emplaceErrorMessage(exception.what());
                }
            });

        api.store(parameters).get();
    }
    catch (const std::exception& exception)
    {
        const QString message = exception.what();
        enumerateVcaEntries(vca,
            [&](auto* entry, auto&&) { entry->emplaceErrorMessage(message); });
    }
}


template <typename Settings, typename Visitor>
void enumerateEntries(Settings* settings, Visitor visit)
{
    const auto uniqueVisit =
        [&](auto* entry)
        {
            visit(QString(entry->name), entry);
        };

    const auto repeatedVisit =
        [&](std::size_t index, auto* entry)
        {
            visit(QString(entry->name).replace("#", "%1").arg(index + 1), entry);
        };

    if (auto& vca = settings->vca)
    {
        uniqueVisit(&vca->enabled);
        uniqueVisit(&vca->sensitivity);
        uniqueVisit(&vca->installation.height);
        uniqueVisit(&vca->installation.tiltAngle);
        uniqueVisit(&vca->installation.rollAngle);

        if (auto& intrusionDetection = vca->intrusionDetection)
        {
            auto& rules = intrusionDetection->rules;
            for (std::size_t i = 0; i < rules.size(); ++i)
            {
                auto& rule = rules[i];

                repeatedVisit(i, &rule.region);
                repeatedVisit(i, &rule.inverted);
            }
        }
    }
}


void parseEntryFromServer(const QString& /*name*/,
    CameraSettings::Entry<bool>* entry, const QString& unparsedValue)
{
    if (unparsedValue == "false")
        entry->emplaceValue(false);
    else if (unparsedValue == "true")
        entry->emplaceValue(true);
    else
        throw Exception("Failed to parse boolean");
}

void parseEntryFromServer(const QString& /*name*/,
    CameraSettings::Vca::Sensitivity* entry, const QString& unparsedValue)
{
    if (unparsedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(unparsedValue), 1, 10));
}

void parseEntryFromServer(const QString& /*name*/,
    CameraSettings::Vca::Installation::Height* entry, const QString& unparsedValue)
{
    if (unparsedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(unparsedValue), 0, 2000));
}

void parseEntryFromServer(const QString& /*name*/,
    CameraSettings::Vca::Installation::TiltAngle* entry, const QString& unparsedValue)
{
    if (unparsedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(unparsedValue), 0, 179));
}

void parseEntryFromServer(const QString& /*name*/,
    CameraSettings::Vca::Installation::RollAngle* entry, const QString& unparsedValue)
{
    if (unparsedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(unparsedValue), -74, +74));
}

void parseEntryFromServer(const QString& /*name*/,
    CameraSettings::Entry<int>* entry, const QString& unparsedValue)
{
    entry->emplaceValue(toInt(unparsedValue));
}

bool parseFromServer(NamedPointSequence* points, const QJsonValue& json)
{
    const auto figure = get<QJsonValue>(json, "figure");
    if (figure.isNull())
        return false;

    points->name = get<QString>(json, "label");
    if (points->name.isEmpty())
        throw Exception("Empty label");

    const auto jsonPoints = get<QJsonArray>("$.figure", figure, "points");
    for (int i = 0; i < jsonPoints.count(); ++i)
    {
        const auto point = get<QJsonArray>("$.figure.points", jsonPoints, i);
        const QString path = NX_FMT("$.figure.points[%1]", i);
        points->emplace_back(
            get<double>(path, point, 0),
            get<double>(path, point, 1));
    }
    
    return true;
}

void parseEntryFromServer(const QString& /*name*/,
    CameraSettings::Entry<NamedPolygon>* entry, const QString& unparsedValue)
{
    const auto json = parseJson(unparsedValue.toUtf8());

    NamedPolygon polygon;
    if (!parseFromServer(&polygon, json))
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::move(polygon));
}


std::optional<QString> unparseEntryToServer(
    const QString& /*name*/, const CameraSettings::Entry<bool>& entry)
{
    if (!entry.hasValue())
        return std::nullopt;

    return entry.value() ? "true" : "false";
}

std::optional<QString> unparseEntryToServer(const QString& /*name*/,
    const CameraSettings::Vca::Sensitivity& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> unparseEntryToServer(const QString& /*name*/,
    const CameraSettings::Vca::Installation::Height& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> unparseEntryToServer(const QString& /*name*/,
    const CameraSettings::Vca::Installation::TiltAngle& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> unparseEntryToServer(const QString& /*name*/,
    const CameraSettings::Vca::Installation::RollAngle& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> unparseEntryToServer(
    const QString& /*name*/, const CameraSettings::Entry<int>& entry)
{
    if (!entry.hasValue())
        return std::nullopt;

    return QString::number(entry.value());
}

QJsonValue unparseToServer(const NamedPointSequence* points)
{
    if (!points)
    {
        return QJsonObject{
            {"label", ""},
            {"figure", QJsonValue::Null},
        };
    }

    return QJsonObject{
        {"label", points->name},
        {"figure", QJsonObject{
            {"points",
                [&]()
                {
                    QJsonArray jsonPoints;

                    for (const auto& point: *points)
                        jsonPoints.push_back(QJsonArray{point.x, point.y});

                    return jsonPoints;
                }(),
            },
        }},
    };
}

std::optional<QString> unparseEntryToServer(const QString& /*name*/,
    const CameraSettings::Entry<NamedPolygon>& entry)
{
    const NamedPolygon* polygon = nullptr;
    if (entry.hasValue())
        polygon = &entry.value();

    return unparseJson(unparseToServer(polygon));
}


QJsonValue getVcaInstallationModelForManifest()
{
    using Installation = CameraSettings::Vca::Installation;
    return QJsonObject{
        {"type", "GroupBox"},
        {"caption", "Camera Installation"},
        {"items", QJsonArray{
            QJsonObject{
                {"name", Installation::Height::name},
                // Should really be SpinBox, but need empty state to work around camera not
                // returning settings for disabled functionality.
                {"type", "TextField"},
                {"caption", "Height (cm)"},
                {"description", "Distance between camera and floor"},
            },
            QJsonObject{
                {"name", Installation::TiltAngle::name},
                // Should really be SpinBox, but need empty state to work around camera not
                // returning settings for disabled functionality.
                {"type", "TextField"},
                {"caption", "Tilt angle (°)"},
                {"description", "Angle between camera direction axis and down direction"},
            },
            QJsonObject{
                {"name", Installation::RollAngle::name},
                // Should really be SpinBox, but need empty state to work around camera not
                // returning settings for disabled functionality.
                {"type", "TextField"},
                {"caption", "Roll angle (°)"},
                {"description", "Angle of camera rotation around direction axis"},
            },
        }},
    };
}

QJsonValue getVcaIntrusionDetectionModelForManifest()
{
    using Rule = CameraSettings::Vca::IntrusionDetection::Rule;
    return QJsonObject{
        {"name", "IntrusionDetection"},
        {"type", "Section"},
        {"caption", "Intrusion Detection"},
        {"items", QJsonArray{
            QJsonObject{
                {"type", "Repeater"},
                {"startIndex", 1},
                {"count", (int) kMaxDetectionRuleCount},
                {"template", QJsonObject{
                    {"type", "GroupBox"},
                    {"caption", "Rule #"},
                    {"filledCheckItems", QJsonArray{Rule::Region::name}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", Rule::Region::name},
                            {"type", "PolygonFigure"},
                            {"caption", "Detection area"},
                            {"minPoints", 3},
                            {"maxPoints", 20}, // Defined in user guide.
                        },
                        QJsonObject{
                            {"name", Rule::Inverted::name},
                            {"type", "ComboBox"},
                            {"caption", "Direction"},
                            {"description", "Movement direction which causes the event"},
                            {"range", QJsonArray{"false", "true"}},
                            {"itemCaptions", QJsonObject{
                                {"false", "In"},
                                {"true", "Out"},
                            }},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonValue getVcaModelForManifest(const CameraFeatures::Vca& features)
{
    using Vca = CameraSettings::Vca;
    return QJsonObject{
        {"name", "Vca"},
        {"type", "Section"},
        {"caption", "Deep Learning VCA"},
        {"items", QJsonArray{
            QJsonObject{
                {"name", Vca::Enabled::name},
                {"type", "SwitchButton"},
                {"caption", "Enabled"},
            },
            QJsonObject{
                {"name", Vca::Sensitivity::name},
                // Should really be SpinBox, but need empty state to work around camera not
                // returning settings for disabled functionality.
                {"type", "TextField"}, 
                {"caption", "Sensitivity"},
                {"description", "The higher the value the more likely an object will be detected as human"},
            },
            getVcaInstallationModelForManifest(),
        }},
        {"sections",
            [&]()
            {
                QJsonArray sections;

                if (features.intrusionDetection)
                    sections.push_back(getVcaIntrusionDetectionModelForManifest());

                return sections;
            }(),
        },
    };
}

} //namespace

CameraSettings::CameraSettings(const CameraFeatures& features)
{
    if (features.vca)
    {
        vca.emplace();

        if (features.vca->intrusionDetection)
        {
            auto& instrusionDetection = vca->intrusionDetection.emplace();
            instrusionDetection.rules.resize(kMaxDetectionRuleCount);
        }
    }
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
        [&](const QString& name, auto* entry)
        {
            try
            {
                const char* unparsedValue = values.value(name.toStdString().data());
                if (!unparsedValue)
                {
                    entry->emplaceNothing();
                    return;
                }

                parseEntryFromServer(name, entry, unparsedValue);
            }
            catch (const std::exception& exception)
            {
                entry->emplaceErrorMessage(exception.what());
            }
        });
}

Ptr<StringMap> CameraSettings::unparseToServer()
{
    auto values = makePtr<StringMap>();

    enumerateEntries(this,
        [&](const QString& name, auto* entry)
        {
            try
            {
                if (const auto unparsedValue = unparseEntryToServer(name, *entry))
                    values->setItem(name.toStdString(), unparsedValue->toStdString());
            }
            catch (const std::exception& exception)
            {
                entry->emplaceErrorMessage(exception.what());
            }
        });

    return values;
}

Ptr<StringMap> CameraSettings::getErrorMessages() const
{
    auto errorMessages = makePtr<StringMap>();

    enumerateEntries(this,
        [&](const QString& name, const auto* entry)
        {
            if (!entry->hasError())
                return;

            errorMessages->setItem(name.toStdString(), entry->errorMessage().toStdString());
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
                    sections.push_back(getVcaModelForManifest(*features.vca));

                return sections;
            }(),
        },
    };
}

} // namespace nx::vms_server_plugins::analytics::vivotek
