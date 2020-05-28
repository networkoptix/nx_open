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


template <typename Visitor>
void enumerateAeEntries(CameraSettings::Vca* vca, Visitor visit)
{
    visit(&vca->sensitivity, "Sensitivity");
    visit(&vca->installation.height, "CamHeight");
    visit(&vca->installation.tiltAngle, "TiltAngle");
    visit(&vca->installation.rollAngle, "RollAngle");
}


template <typename Value>
void fillReErrors(CameraSettings::Entry<Value>* entry,
    const QString& message)
{
    entry->emplaceErrorMessage(message);
}

void fillReErrors(CameraSettings::Vca::IntrusionDetection* intrusionDetection,
    const QString& message)
{
    for (auto& rule: intrusionDetection->rules)
    {
        fillReErrors(&rule.region, message);
        fillReErrors(&rule.inverted, message);
    }
}


void fillReErrors(CameraSettings::Vca* vca, const QString& message)
{
    if (auto& intrusionDetection = vca->intrusionDetection)
        fillReErrors(&*intrusionDetection, message);
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


template <typename... Keys>
void parseAeFromCamera(CameraSettings::Vca::Installation::Height* entry,
    const QJsonValue& parameters, const Keys&... keys)
{
    using Value = CameraSettings::Vca::Installation::Height::Value;
    // API uses millimeters, while web UI uses centimeters.
    entry->emplaceValue(get<Value>(parameters, keys...) / 10);
}

template <typename Value, typename... Keys>
void parseAeFromCamera(CameraSettings::Entry<Value>* entry,
    const QJsonValue& parameters, const Keys&... keys)
{
    entry->emplaceValue(get<Value>(parameters, keys...));
}


void fetchAeFromCamera(CameraSettings::Vca* vca, const Url& cameraUrl)
{
    try
    {
        CameraVcaParameterApi api(cameraUrl);

        const auto parameters = api.fetch("Config/AE").get();

        enumerateAeEntries(vca,
            [&](auto* entry, const auto&... keys)
            {
                try
                {
                    parseAeFromCamera(entry, parameters, keys...);
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
        enumerateAeEntries(vca,
            [&](auto* entry, auto&&...) { entry->emplaceErrorMessage(message); });
    }
}

void parseReFromCamera(CameraSettings::Vca::IntrusionDetection::Rule::Region* region,
    const QJsonValue& rule, const QString& ruleName, const QString& path)
{
    try
    {
        auto& value = region->emplaceValue();

        value.name = ruleName;

        // That last 0 is intentional. For some reason, camera returns each region as an array of
        // a single array of points.
        const auto field = get<QJsonArray>(path, rule, "Field", 0);
        for (int i = 0; i < field.size(); ++i)
        {
            value.push_back(CameraVcaParameterApi::parsePoint(
                field[i], NX_FMT("%1.Field[%2]", path, i)));
        }
    }
    catch (const std::exception& exception)
    {
        region->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::IntrusionDetection::Rule::Inverted* inverted,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        const auto walkingDirection = get<QString>(path, rule, "WalkingDirection");
        if (walkingDirection == "OutToIn")
            inverted->emplaceValue(false);
        else if (walkingDirection == "InToOut")
            inverted->emplaceValue(true);
        else
        {
            throw Exception("%1.WalkingDirection has unexpected value: %2",
                path, walkingDirection);
        }
    }
    catch (const std::exception& exception)
    {
        inverted->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::IntrusionDetection* intrusionDetection,
    const QJsonValue& parameters)
{
    try
    {
        QJsonObject jsonRules;
        if (!get<QJsonValue>(parameters, "IntrusionDetection").isUndefined())
            jsonRules = get<QJsonObject>(parameters, "IntrusionDetection");

        const auto ruleNames = jsonRules.keys();

        auto& rules = intrusionDetection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];
            if (i >= (std::size_t) ruleNames.size())
            {
                rule.region.emplaceNothing();
                rule.inverted.emplaceNothing();
                continue;
            }

            const auto& ruleName = ruleNames[i];
            const auto& jsonRule = jsonRules[ruleName];

            const QString path = NX_FMT("$.IntrusionDetection.%1", ruleName);

            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
            parseReFromCamera(&rule.inverted, jsonRule, path);
        }
    }
    catch (const std::exception& exception)
    {
        fillReErrors(intrusionDetection, exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca* vca, const QJsonValue& parameters)
{
    if (auto& intrusionDetection = vca->intrusionDetection)
        parseReFromCamera(&*intrusionDetection, parameters);
}


void fetchReFromCamera(CameraSettings::Vca* vca, const Url& cameraUrl)
{
    try
    {
        CameraVcaParameterApi api(cameraUrl);

        const auto parameters = api.fetch("Config/RE").get();

        parseReFromCamera(vca, parameters);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(vca, exception.what());
    }
}

void fetchFromCamera(CameraSettings::Vca* vca, const Url& cameraUrl)
{
    auto& enabled = vca->enabled;
    fetchFromCamera(&enabled, cameraUrl);
    if (!enabled.hasValue() || !enabled.value())
        return;

    fetchAeFromCamera(vca, cameraUrl);
    fetchReFromCamera(vca, cameraUrl);
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

        // Work around camera not inserting \r\n between response headers and body.
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


auto unparseAeToCamera(const CameraSettings::Vca::Installation::Height& entry)
{
    return entry.value() * 10; // API uses millimeters, while web UI uses centimeters.
}

template <typename Value>
auto unparseAeToCamera(const CameraSettings::Entry<Value>& entry)
{
    return entry.value();
}


void storeAeToCamera(const Url& cameraUrl, CameraSettings::Vca* vca)
{
    try
    {
        CameraVcaParameterApi api(cameraUrl);

        auto parameters = api.fetch("Config/AE").get();

        enumerateAeEntries(vca,
            [&](auto* entry, const auto&... keys)
            {
                try
                {
                    if (!entry->hasValue())
                        return;

                    set(&parameters, keys..., unparseAeToCamera(*entry));
                }
                catch (const std::exception& exception)
                {
                    entry->emplaceErrorMessage(exception.what());
                }
            });

        api.store("Config/AE", parameters).get();

        api.reloadConfig().get();
    }
    catch (const std::exception& exception)
    {
        const QString message = exception.what();
        enumerateAeEntries(vca,
            [&](auto* entry, auto&&...) { entry->emplaceErrorMessage(message); });
    }
}


std::optional<QString> getName(const CameraSettings::Entry<NamedPolygon>& entry)
{
    if (!entry.hasValue())
        return std::nullopt;

    const auto& region = entry.value();
    return region.name;
}

std::optional<QString> getName(const CameraSettings::Vca::IntrusionDetection::Rule& rule)
{
    return getName(rule.region);
}


void unparseReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::IntrusionDetection::Rule::Region* region)
{
    try
    {
        if (!region->hasValue())
            return;

        set(jsonRule, "RuleName", region->value().name);
        set(jsonRule, "EventName", region->value().name);

        set(jsonRule, "Field3D", QJsonValue::Undefined);

        QJsonArray field;
        for (const auto& point: region->value())
            field.push_back(CameraVcaParameterApi::unparse(point));

        // Wrapping in another array is intentional. For some reason, camera expects each region
        // as an array of a single array of points.
        set(jsonRule, "Field", QJsonArray{field});
    }
    catch (const std::exception& exception)
    {
        fillReErrors(region, exception.what());
    }
}

void unparseReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::IntrusionDetection::Rule::Inverted* inverted)
{
    try
    {
        // If new rule, set default like in web UI.
        if (inverted->hasNothing()
            && get<QJsonValue>(*jsonRule, "WalkingDirection").isUndefined())
        {
            inverted->emplaceValue(false);
        }

        if (!inverted->hasValue())
            return;

        set(jsonRule, "WalkingDirection", inverted->value() ? "InToOut" : "OutToIn");
    }
    catch (const std::exception& exception)
    {
        fillReErrors(inverted, exception.what());
    }
}

void unparseReToCamera(QJsonValue* parameters,
    CameraSettings::Vca::IntrusionDetection* intrusionDetection)
{
    try
    {
        auto& rules = intrusionDetection->rules;

        QJsonObject jsonRules;
        if (!get<QJsonValue>(*parameters, "IntrusionDetection").isUndefined())
        {
            jsonRules = get<QJsonObject>(*parameters, "IntrusionDetection");

            std::set<QString> ruleNames;
            for (const auto& rule: rules)
            {
                if (auto name = getName(rule))
                    ruleNames.insert(*name);
            }
            for (const auto& oldName: jsonRules.keys())
            {
                if (!ruleNames.count(oldName))
                    jsonRules.remove(oldName);
            }
        }

        for (auto& rule: rules)
        {
            if (auto name = getName(rule))
            {
                QJsonValue jsonRule = jsonRules[*name];
                if (jsonRule.isUndefined() || jsonRule.isNull())
                    jsonRule = QJsonObject{};

                unparseReToCamera(&jsonRule, &rule.region);
                unparseReToCamera(&jsonRule, &rule.inverted);

                jsonRules[*name] = jsonRule;
            }
        }

        set(parameters, "IntrusionDetection", jsonRules);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(intrusionDetection, exception.what());
    }
}

void unparseReToCamera(QJsonValue* parameters, CameraSettings::Vca* vca)
{
    if (auto& intrusionDetection = vca->intrusionDetection)
        unparseReToCamera(parameters, &*intrusionDetection);
}


void storeReToCamera(const Url& cameraUrl, CameraSettings::Vca* vca)
{
    try
    {
        CameraVcaParameterApi api(cameraUrl);

        auto parameters = api.fetch("Config/RE").get();

        unparseReToCamera(&parameters, vca);

        api.store("Config/RE", parameters).get();

        api.reloadConfig().get();
    }
    catch (const std::exception& exception)
    {
        fillReErrors(vca, exception.what());
    }
}


void storeToCamera(const Url& cameraUrl, CameraSettings::Vca* vca)
{
    auto& enabled = vca->enabled;
    storeToCamera(cameraUrl, &enabled);
    if (!enabled.hasValue() || !enabled.value())
        return;

    storeAeToCamera(cameraUrl, vca);
    storeReToCamera(cameraUrl, vca);
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


void parseEntryFromServer(
    CameraSettings::Vca::IntrusionDetection::Rule::Inverted* entry, const QString& unparsedValue)
{
    if (unparsedValue == "")
        entry->emplaceNothing();
    else if (unparsedValue == "false")
        entry->emplaceValue(false);
    else if (unparsedValue == "true")
        entry->emplaceValue(true);
    else
        throw Exception("Failed to parse boolean");
}

void parseEntryFromServer(
    CameraSettings::Entry<bool>* entry, const QString& unparsedValue)
{
    if (unparsedValue == "false")
        entry->emplaceValue(false);
    else if (unparsedValue == "true")
        entry->emplaceValue(true);
    else
        throw Exception("Failed to parse boolean");
}

void parseEntryFromServer(
    CameraSettings::Vca::Sensitivity* entry, const QString& unparsedValue)
{
    if (unparsedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(unparsedValue), 1, 10));
}

void parseEntryFromServer(
    CameraSettings::Vca::Installation::Height* entry, const QString& unparsedValue)
{
    if (unparsedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(unparsedValue), 0, 2000));
}

void parseEntryFromServer(
    CameraSettings::Vca::Installation::TiltAngle* entry, const QString& unparsedValue)
{
    if (unparsedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(unparsedValue), 0, 179));
}

void parseEntryFromServer(
    CameraSettings::Vca::Installation::RollAngle* entry, const QString& unparsedValue)
{
    if (unparsedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(unparsedValue), -74, +74));
}

void parseEntryFromServer(CameraSettings::Entry<int>* entry, const QString& unparsedValue)
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

void parseEntryFromServer(
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
    const CameraSettings::Vca::IntrusionDetection::Rule::Inverted& entry)
{
    if (!entry.hasValue())
        return "";

    return entry.value() ? "true" : "false";
}

std::optional<QString> unparseEntryToServer(const CameraSettings::Entry<bool>& entry)
{
    if (!entry.hasValue())
        return std::nullopt;

    return entry.value() ? "true" : "false";
}

std::optional<QString> unparseEntryToServer(const CameraSettings::Vca::Sensitivity& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> unparseEntryToServer(
    const CameraSettings::Vca::Installation::Height& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> unparseEntryToServer(
    const CameraSettings::Vca::Installation::TiltAngle& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> unparseEntryToServer(
    const CameraSettings::Vca::Installation::RollAngle& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> unparseEntryToServer(const CameraSettings::Entry<int>& entry)
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

std::optional<QString> unparseEntryToServer(const CameraSettings::Entry<NamedPolygon>& entry)
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
                            {"range", QJsonArray{
                                // Need empty state to work around camera not returning settings
                                // for disabled functionality.
                                "",
                                "false",
                                "true",
                            }},
                            {"itemCaptions", QJsonObject{
                                {"", ""},
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
            auto& intrusionDetection = vca->intrusionDetection.emplace();
            intrusionDetection.rules.resize(kMaxDetectionRuleCount);
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

                parseEntryFromServer(entry, unparsedValue);
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
                if (const auto unparsedValue = unparseEntryToServer(*entry))
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
