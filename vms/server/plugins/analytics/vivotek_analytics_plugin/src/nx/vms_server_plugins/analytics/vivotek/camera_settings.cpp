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
using namespace nx::sdk::analytics;
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
    static bool parseStatus(const QString& serializedValue)
    {
        if (serializedValue == "off")
            return false;
        if (serializedValue == "on")
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

void fillReErrors(CameraSettings::Vca::CrowdDetection* crowdDetection,
    const QString& message)
{
    for (auto& rule: crowdDetection->rules)
    {
        fillReErrors(&rule.region, message);
        fillReErrors(&rule.sizeThreshold, message);
        fillReErrors(&rule.enterDelay, message);
        fillReErrors(&rule.exitDelay, message);
    }
}

void fillReErrors(CameraSettings::Vca::LoiteringDetection* loiteringDetection,
    const QString& message)
{
    for (auto& rule: loiteringDetection->rules)
    {
        fillReErrors(&rule.region, message);
        fillReErrors(&rule.delay, message);
    }
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

void fillReErrors(CameraSettings::Vca::LineCrossingDetection* lineCrossingDetection,
    const QString& message)
{
    for (auto& rule: lineCrossingDetection->rules)
    {
        fillReErrors(&rule.line, message);
    }
}

void fillReErrors(CameraSettings::Vca::MissingObjectDetection* missingObjectDetection,
    const QString& message)
{
    for (auto& rule: missingObjectDetection->rules)
    {
        fillReErrors(&rule.region, message);
        fillReErrors(&rule.minSize, message);
        fillReErrors(&rule.maxSize, message);
        fillReErrors(&rule.delay, message);
        fillReErrors(&rule.requiresHumanInvolvement, message);
    }
}

void fillReErrors(CameraSettings::Vca::UnattendedObjectDetection* unattendedObjectDetection,
    const QString& message)
{
    for (auto& rule: unattendedObjectDetection->rules)
    {
        fillReErrors(&rule.region, message);
        fillReErrors(&rule.minSize, message);
        fillReErrors(&rule.maxSize, message);
        fillReErrors(&rule.delay, message);
        fillReErrors(&rule.requiresHumanInvolvement, message);
    }
}

void fillReErrors(CameraSettings::Vca::FaceDetection* faceDetection,
    const QString& message)
{
    for (auto& rule: faceDetection->rules)
    {
        fillReErrors(&rule.region, message);
    }
}

void fillReErrors(CameraSettings::Vca* vca, const QString& message)
{
    if (auto& crowdDetection = vca->crowdDetection)
        fillReErrors(&*crowdDetection, message);
    if (auto& loiteringDetection = vca->loiteringDetection)
        fillReErrors(&*loiteringDetection, message);
    if (auto& intrusionDetection = vca->intrusionDetection)
        fillReErrors(&*intrusionDetection, message);
    if (auto& lineCrossingDetection = vca->lineCrossingDetection)
        fillReErrors(&*lineCrossingDetection, message);
    if (auto& unattendedObjectDetection = vca->unattendedObjectDetection)
        fillReErrors(&*unattendedObjectDetection, message);
    if (auto& faceDetection = vca->faceDetection)
        fillReErrors(&*faceDetection, message);
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


void fetchAeFromCamera(CameraSettings::Vca* vca, CameraVcaParameterApi* api)
{
    try
    {
        const auto parameters = api->fetch("Config/AE").get();

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

void parseReFromCamera(CameraSettings::Entry<NamedPolygon>* region,
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

void parseReFromCamera(CameraSettings::Vca::CrowdDetection::Rule::SizeThreshold* sizeThreshold,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        sizeThreshold->emplaceValue(get<int>(path, rule, "PeopleNumber"));
    }
    catch (const std::exception& exception)
    {
        sizeThreshold->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::CrowdDetection::Rule::EnterDelay* enterDelay,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        enterDelay->emplaceValue(get<int>(path, rule, "EnterDelay"));
    }
    catch (const std::exception& exception)
    {
        enterDelay->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::CrowdDetection::Rule::ExitDelay* exitDelay,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        exitDelay->emplaceValue(get<int>(path, rule, "LeaveDelay"));
    }
    catch (const std::exception& exception)
    {
        exitDelay->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::CrowdDetection* crowdDetection,
    const QJsonValue& parameters)
{
    try
    {
        QJsonObject jsonRules;
        if (!get<QJsonValue>(parameters, "CrowdDetection").isUndefined())
            get(&jsonRules, parameters, "CrowdDetection");

        const auto ruleNames = jsonRules.keys();

        auto& rules = crowdDetection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];
            if (i >= (std::size_t) ruleNames.size())
            {
                rule.region.emplaceNothing();
                rule.sizeThreshold.emplaceNothing();
                rule.enterDelay.emplaceNothing();
                rule.exitDelay.emplaceNothing();
                continue;
            }

            const auto& ruleName = ruleNames[i];
            const auto& jsonRule = jsonRules[ruleName];

            const QString path = NX_FMT("$.CrowdDetection.%1", ruleName);

            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
            parseReFromCamera(&rule.sizeThreshold, jsonRule, path);
            parseReFromCamera(&rule.enterDelay, jsonRule, path);
            parseReFromCamera(&rule.exitDelay, jsonRule, path);
        }
    }
    catch (const std::exception& exception)
    {
        fillReErrors(crowdDetection, exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::LoiteringDetection::Rule::Delay* delay,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        delay->emplaceValue(get<int>(path, rule, "StayTime"));
    }
    catch (const std::exception& exception)
    {
        delay->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::LoiteringDetection* loiteringDetection,
    const QJsonValue& parameters)
{
    try
    {
        QJsonObject jsonRules;
        if (!get<QJsonValue>(parameters, "LoiteringDetection").isUndefined())
            get(&jsonRules, parameters, "LoiteringDetection");

        const auto ruleNames = jsonRules.keys();

        auto& rules = loiteringDetection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];
            if (i >= (std::size_t) ruleNames.size())
            {
                rule.region.emplaceNothing();
                rule.delay.emplaceNothing();
                continue;
            }

            const auto& ruleName = ruleNames[i];
            const auto& jsonRule = jsonRules[ruleName];

            const QString path = NX_FMT("$.LoiteringDetection.%1", ruleName);

            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
            parseReFromCamera(&rule.delay, jsonRule, path);
        }
    }
    catch (const std::exception& exception)
    {
        fillReErrors(loiteringDetection, exception.what());
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
            get(&jsonRules, parameters, "IntrusionDetection");

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

void parseReFromCamera(NamedLine::Direction* direction,
    const QJsonValue& rule, const QString& path)
{
    const auto nativeDirection = get<QString>(path, rule, "Direction");
    if (nativeDirection == "Any")
        *direction = NamedLine::Direction::any;
    else if (nativeDirection == "Out")
        *direction = NamedLine::Direction::leftToRight;
    else if (nativeDirection == "In")
        *direction = NamedLine::Direction::rightToLeft;
    else
        throw Exception("Unexpected value of %1.Direction: %2", path, nativeDirection);
}

void parseReFromCamera(CameraSettings::Entry<NamedLine>* line,
    const QJsonValue& rule, const QString& ruleName, const QString& path)
{
    try
    {
        auto& value = line->emplaceValue();

        value.name = ruleName;

        // That last 0 is intentional. For some reason, camera returns each line as an array of
        // a single array of points.
        const auto jsonLine = get<QJsonArray>(path, rule, "Line", 0);
        for (int i = 0; i < jsonLine.size(); ++i)
        {
            value.push_back(CameraVcaParameterApi::parsePoint(
                jsonLine[i], NX_FMT("%1.Line[%2]", path, i)));
        }

        parseReFromCamera(&value.direction, rule, path);
    }
    catch (const std::exception& exception)
    {
        line->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::LineCrossingDetection* lineCrossingDetection,
    const QJsonValue& parameters)
{
    try
    {
        QJsonObject jsonRules;
        if (!get<QJsonValue>(parameters, "LineCrossingDetection").isUndefined())
            get(&jsonRules, parameters, "LineCrossingDetection");

        const auto ruleNames = jsonRules.keys();

        auto& rules = lineCrossingDetection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];
            if (i >= (std::size_t) ruleNames.size())
            {
                rule.line.emplaceNothing();
                continue;
            }

            const auto& ruleName = ruleNames[i];
            const auto& jsonRule = jsonRules[ruleName];

            const QString path = NX_FMT("$.LineCrossingDetection.%1", ruleName);

            parseReFromCamera(&rule.line, jsonRule, ruleName, path);
        }
    }
    catch (const std::exception& exception)
    {
        fillReErrors(lineCrossingDetection, exception.what());
    }
}

void parseReFromCamera(Rect* rect, const QJsonObject& json, const QString& path)
{
    const auto parseCoordinate =
        [&](const QString& key)
        {
            return CameraVcaParameterApi::parseCoordinate(
                get<QJsonValue>(path, json, key),
                NX_FMT("%1.%2", path, key));
        };

    rect->x = parseCoordinate("x");
    rect->y = parseCoordinate("y");
    rect->width = parseCoordinate("w");
    rect->height = parseCoordinate("h");
}

void parseReFromCamera(CameraSettings::Vca::MissingObjectDetection::Rule::MinSize* minSize,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        parseReFromCamera(&minSize->emplaceValue(),
            get<QJsonObject>(path, rule, "AreaLimitation", "Minimum"),
            NX_FMT("%1.AreaLimitation.Minimum", path));
    }
    catch (const std::exception& exception)
    {
        minSize->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::MissingObjectDetection::Rule::MaxSize* maxSize,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        parseReFromCamera(&maxSize->emplaceValue(),
            get<QJsonObject>(path, rule, "AreaLimitation", "Maximum"),
            NX_FMT("%1.AreaLimitation.Maximum", path));
    }
    catch (const std::exception& exception)
    {
        maxSize->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::MissingObjectDetection::Rule::Delay* delay,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        delay->emplaceValue(get<int>(path, rule, "Duration"));
    }
    catch (const std::exception& exception)
    {
        delay->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(
    CameraSettings::Vca::MissingObjectDetection::Rule::RequiresHumanInvolvement* requiresHumanInvolvement,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        requiresHumanInvolvement->emplaceValue(get<bool>(path, rule, "HumanFactor"));
    }
    catch (const std::exception& exception)
    {
        requiresHumanInvolvement->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::MissingObjectDetection* missingObjectDetection,
    const QJsonValue& parameters)
{
    try
    {
        QJsonObject jsonRules;
        if (!get<QJsonValue>(parameters, "MissingObjectDetection").isUndefined())
            get(&jsonRules, parameters, "MissingObjectDetection");

        const auto ruleNames = jsonRules.keys();

        auto& rules = missingObjectDetection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];
            if (i >= (std::size_t) ruleNames.size())
            {
                rule.region.emplaceNothing();
                rule.minSize.emplaceNothing();
                rule.maxSize.emplaceNothing();
                rule.delay.emplaceNothing();
                rule.requiresHumanInvolvement.emplaceNothing();
                continue;
            }

            const auto& ruleName = ruleNames[i];
            const auto& jsonRule = jsonRules[ruleName];

            const QString path = NX_FMT("$.MissingObjectDetection.%1", ruleName);

            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
            parseReFromCamera(&rule.minSize, jsonRule, path);
            parseReFromCamera(&rule.maxSize, jsonRule, path);
            parseReFromCamera(&rule.delay, jsonRule, path);
            parseReFromCamera(&rule.requiresHumanInvolvement, jsonRule, path);
        }
    }
    catch (const std::exception& exception)
    {
        fillReErrors(missingObjectDetection, exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::UnattendedObjectDetection::Rule::MinSize* minSize,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        parseReFromCamera(&minSize->emplaceValue(),
            get<QJsonObject>(path, rule, "AreaLimitation", "Minimum"),
            NX_FMT("%1.AreaLimitation.Minimum", path));
    }
    catch (const std::exception& exception)
    {
        minSize->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::UnattendedObjectDetection::Rule::MaxSize* maxSize,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        parseReFromCamera(&maxSize->emplaceValue(),
            get<QJsonObject>(path, rule, "AreaLimitation", "Maximum"),
            NX_FMT("%1.AreaLimitation.Maximum", path));
    }
    catch (const std::exception& exception)
    {
        maxSize->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::UnattendedObjectDetection::Rule::Delay* delay,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        delay->emplaceValue(get<int>(path, rule, "Duration"));
    }
    catch (const std::exception& exception)
    {
        delay->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(
    CameraSettings::Vca::UnattendedObjectDetection::Rule::RequiresHumanInvolvement* requiresHumanInvolvement,
    const QJsonValue& rule, const QString& path)
{
    try
    {
        requiresHumanInvolvement->emplaceValue(get<bool>(path, rule, "HumanFactor"));
    }
    catch (const std::exception& exception)
    {
        requiresHumanInvolvement->emplaceErrorMessage(exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::UnattendedObjectDetection* unattendedObjectDetection,
    const QJsonValue& parameters)
{
    try
    {
        QJsonObject jsonRules;
        if (!get<QJsonValue>(parameters, "UnattendedObjectDetection").isUndefined())
            get(&jsonRules, parameters, "UnattendedObjectDetection");

        const auto ruleNames = jsonRules.keys();

        auto& rules = unattendedObjectDetection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];
            if (i >= (std::size_t) ruleNames.size())
            {
                rule.region.emplaceNothing();
                rule.minSize.emplaceNothing();
                rule.maxSize.emplaceNothing();
                rule.delay.emplaceNothing();
                rule.requiresHumanInvolvement.emplaceNothing();
                continue;
            }

            const auto& ruleName = ruleNames[i];
            const auto& jsonRule = jsonRules[ruleName];

            const QString path = NX_FMT("$.UnattendedObjectDetection.%1", ruleName);

            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
            parseReFromCamera(&rule.minSize, jsonRule, path);
            parseReFromCamera(&rule.maxSize, jsonRule, path);
            parseReFromCamera(&rule.delay, jsonRule, path);
            parseReFromCamera(&rule.requiresHumanInvolvement, jsonRule, path);
        }
    }
    catch (const std::exception& exception)
    {
        fillReErrors(unattendedObjectDetection, exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca::FaceDetection* faceDetection,
    const QJsonValue& parameters)
{
    try
    {
        QJsonObject jsonRules;
        if (!get<QJsonValue>(parameters, "FaceDetection").isUndefined())
            get(&jsonRules, parameters, "FaceDetection");

        const auto ruleNames = jsonRules.keys();

        auto& rules = faceDetection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];
            if (i >= (std::size_t) ruleNames.size())
            {
                rule.region.emplaceNothing();
                continue;
            }

            const auto& ruleName = ruleNames[i];
            const auto& jsonRule = jsonRules[ruleName];

            const QString path = NX_FMT("$.FaceDetection.%1", ruleName);

            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
        }
    }
    catch (const std::exception& exception)
    {
        fillReErrors(faceDetection, exception.what());
    }
}

void parseReFromCamera(CameraSettings::Vca* vca, const QJsonValue& parameters)
{
    if (auto& crowdDetection = vca->crowdDetection)
        parseReFromCamera(&*crowdDetection, parameters);
    if (auto& loiteringDetection = vca->loiteringDetection)
        parseReFromCamera(&*loiteringDetection, parameters);
    if (auto& intrusionDetection = vca->intrusionDetection)
        parseReFromCamera(&*intrusionDetection, parameters);
    if (auto& lineCrossingDetection = vca->lineCrossingDetection)
        parseReFromCamera(&*lineCrossingDetection, parameters);
    if (auto& missingObjectDetection = vca->missingObjectDetection)
        parseReFromCamera(&*missingObjectDetection, parameters);
    if (auto& unattendedObjectDetection = vca->unattendedObjectDetection)
        parseReFromCamera(&*unattendedObjectDetection, parameters);
    if (auto& faceDetection = vca->faceDetection)
        parseReFromCamera(&*faceDetection, parameters);
}


void fetchReFromCamera(CameraSettings::Vca* vca, CameraVcaParameterApi* api)
{
    try
    {
        const auto parameters = api->fetch("Config/RE").get();

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

    CameraVcaParameterApi api(cameraUrl);

    fetchAeFromCamera(vca, &api);
    fetchReFromCamera(vca, &api);
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


auto serializeAeToCamera(const CameraSettings::Vca::Installation::Height& entry)
{
    return entry.value() * 10; // API uses millimeters, while web UI uses centimeters.
}

template <typename Value>
auto serializeAeToCamera(const CameraSettings::Entry<Value>& entry)
{
    return entry.value();
}


void storeAeToCamera(CameraVcaParameterApi* api, CameraSettings::Vca* vca)
{
    try
    {
        auto parameters = api->fetch("Config/AE").get();

        enumerateAeEntries(vca,
            [&](auto* entry, const auto&... keys)
            {
                try
                {
                    if (!entry->hasValue())
                        return;

                    set(&parameters, keys..., serializeAeToCamera(*entry));
                }
                catch (const std::exception& exception)
                {
                    entry->emplaceErrorMessage(exception.what());
                }
            });

        api->store("Config/AE", parameters).get();
    }
    catch (const std::exception& exception)
    {
        const QString message = exception.what();
        enumerateAeEntries(vca,
            [&](auto* entry, auto&&...) { entry->emplaceErrorMessage(message); });
    }
}


template <typename NamedThing>
std::optional<QString> getName(const CameraSettings::Entry<NamedThing>& entry)
{
    if (!entry.hasValue())
        return std::nullopt;

    const auto& thing = entry.value();
    return thing.name;
}

template <typename Rule>
auto getName(const Rule& rule) -> decltype(getName(rule.region))
{
    return getName(rule.region);
}

template <typename Rule>
auto getName(const Rule& rule) -> decltype(getName(rule.line))
{
    return getName(rule.line);
}

template <typename Rule>
std::set<QString> getNames(const std::vector<Rule>& rules)
{
    std::set<QString> names;
    for (const auto& rule: rules)
    {
        if (auto name = getName(rule))
            names.insert(*name);
    }
    return names;
}

void removeRulesExcept(QJsonObject* rules, const std::set<QString>& allowedNames)
{
    for (const auto& name: rules->keys())
    {
        if (!allowedNames.count(name))
            rules->remove(name);
    }
}


void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Entry<NamedPolygon>* region)
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
            field.push_back(CameraVcaParameterApi::serialize(point));

        // Wrapping in another array is intentional. For some reason, camera expects each region
        // as an array of a single array of points.
        set(jsonRule, "Field", QJsonArray{field});
    }
    catch (const std::exception& exception)
    {
        fillReErrors(region, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::CrowdDetection::Rule::SizeThreshold* sizeThreshold)
{
    try
    {
        // If new rule, set default like in web UI.
        if (sizeThreshold->hasNothing()
            && get<QJsonValue>(*jsonRule, "PeopleNumber").isUndefined())
        {
            sizeThreshold->emplaceValue(2);
        }

        if (!sizeThreshold->hasValue())
            return;

        set(jsonRule, "PeopleNumber", sizeThreshold->value());
    }
    catch (const std::exception& exception)
    {
        fillReErrors(sizeThreshold, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::CrowdDetection::Rule::EnterDelay* enterDelay)
{
    try
    {
        if (!enterDelay->hasValue())
            return;

        set(jsonRule, "EnterDelay", enterDelay->value());
    }
    catch (const std::exception& exception)
    {
        fillReErrors(enterDelay, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::CrowdDetection::Rule::ExitDelay* exitDelay)
{
    try
    {
        // If new rule, set default like in web UI.
        if (exitDelay->hasNothing()
            && get<QJsonValue>(*jsonRule, "LeaveDelay").isUndefined())
        {
            exitDelay->emplaceValue(1);
        }

        if (!exitDelay->hasValue())
            return;

        set(jsonRule, "LeaveDelay", exitDelay->value());
    }
    catch (const std::exception& exception)
    {
        fillReErrors(exitDelay, exception.what());
    }
}

void serializeReToCamera(QJsonValue* parameters,
    CameraSettings::Vca::CrowdDetection* crowdDetection)
{
    try
    {
        auto& rules = crowdDetection->rules;

        QJsonObject jsonRules;
        if (!get<QJsonValue>(*parameters, "CrowdDetection").isUndefined())
        {
            jsonRules = get<QJsonObject>(*parameters, "CrowdDetection");
            removeRulesExcept(&jsonRules, getNames(rules));
        }

        for (auto& rule: rules)
        {
            if (auto name = getName(rule))
            {
                QJsonValue jsonRule = jsonRules[*name];
                if (jsonRule.isUndefined() || jsonRule.isNull())
                    jsonRule = QJsonObject{};

                serializeReToCamera(&jsonRule, &rule.region);
                serializeReToCamera(&jsonRule, &rule.sizeThreshold);
                serializeReToCamera(&jsonRule, &rule.enterDelay);
                serializeReToCamera(&jsonRule, &rule.exitDelay);

                jsonRules[*name] = jsonRule;
            }
        }

        set(parameters, "CrowdDetection", jsonRules);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(crowdDetection, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::LoiteringDetection::Rule::Delay* delay)
{
    try
    {
        // If new rule, set default like in web UI.
        if (delay->hasNothing()
            && get<QJsonValue>(*jsonRule, "StayTime").isUndefined())
        {
            delay->emplaceValue(5);
        }

        if (!delay->hasValue())
            return;

        set(jsonRule, "StayTime", delay->value());
    }
    catch (const std::exception& exception)
    {
        fillReErrors(delay, exception.what());
    }
}

void serializeReToCamera(QJsonValue* parameters,
    CameraSettings::Vca::LoiteringDetection* loiteringDetection)
{
    try
    {
        auto& rules = loiteringDetection->rules;

        QJsonObject jsonRules;
        if (!get<QJsonValue>(*parameters, "LoiteringDetection").isUndefined())
        {
            jsonRules = get<QJsonObject>(*parameters, "LoiteringDetection");
            removeRulesExcept(&jsonRules, getNames(rules));
        }

        for (auto& rule: rules)
        {
            if (auto name = getName(rule))
            {
                QJsonValue jsonRule = jsonRules[*name];
                if (jsonRule.isUndefined() || jsonRule.isNull())
                    jsonRule = QJsonObject{};

                serializeReToCamera(&jsonRule, &rule.region);
                serializeReToCamera(&jsonRule, &rule.delay);

                jsonRules[*name] = jsonRule;
            }
        }

        set(parameters, "LoiteringDetection", jsonRules);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(loiteringDetection, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
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

void serializeReToCamera(QJsonValue* parameters,
    CameraSettings::Vca::IntrusionDetection* intrusionDetection)
{
    try
    {
        auto& rules = intrusionDetection->rules;

        QJsonObject jsonRules;
        if (!get<QJsonValue>(*parameters, "IntrusionDetection").isUndefined())
        {
            jsonRules = get<QJsonObject>(*parameters, "IntrusionDetection");
            removeRulesExcept(&jsonRules, getNames(rules));
        }

        for (auto& rule: rules)
        {
            if (auto name = getName(rule))
            {
                QJsonValue jsonRule = jsonRules[*name];
                if (jsonRule.isUndefined() || jsonRule.isNull())
                    jsonRule = QJsonObject{};

                serializeReToCamera(&jsonRule, &rule.region);
                serializeReToCamera(&jsonRule, &rule.inverted);

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

QString serializeReToCamera(NamedLine::Direction direction)
{
    switch (direction)
    {
        case NamedLine::Direction::any:
            return "Any";
        case NamedLine::Direction::leftToRight:
            return "Out";
        case NamedLine::Direction::rightToLeft:
            return "In";
        default:
            NX_ASSERT(false, "Unknown NamedLine::Direction value: %1", (int) direction);
            return "";
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Entry<NamedLine>* line)
{
    try
    {
        if (!line->hasValue())
            return;

        set(jsonRule, "RuleName", line->value().name);
        set(jsonRule, "EventName", line->value().name);

        set(jsonRule, "Line3D", QJsonValue::Undefined);

        QJsonArray jsonLine;
        for (const auto& point: line->value())
            jsonLine.push_back(CameraVcaParameterApi::serialize(point));

        // Wrapping in another array is intentional. For some reason, camera expects each line
        // as an array of a single array of points.
        set(jsonRule, "Line", QJsonArray{jsonLine});

        set(jsonRule, "Direction", serializeReToCamera(line->value().direction));
    }
    catch (const std::exception& exception)
    {
        fillReErrors(line, exception.what());
    }
}

void serializeReToCamera(QJsonValue* parameters,
    CameraSettings::Vca::LineCrossingDetection* lineCrossingDetection)
{
    try
    {
        auto& rules = lineCrossingDetection->rules;

        QJsonObject jsonRules;
        if (!get<QJsonValue>(*parameters, "LineCrossingDetection").isUndefined())
        {
            jsonRules = get<QJsonObject>(*parameters, "LineCrossingDetection");
            removeRulesExcept(&jsonRules, getNames(rules));
        }

        for (auto& rule: rules)
        {
            if (auto name = getName(rule))
            {
                QJsonValue jsonRule = jsonRules[*name];
                if (jsonRule.isUndefined() || jsonRule.isNull())
                    jsonRule = QJsonObject{};

                serializeReToCamera(&jsonRule, &rule.line);

                jsonRules[*name] = jsonRule;
            }
        }

        set(parameters, "LineCrossingDetection", jsonRules);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(lineCrossingDetection, exception.what());
    }
}

QJsonObject serializeReToCamera(const Rect& rect)
{
    return QJsonObject{
        {"x", CameraVcaParameterApi::serializeCoordinate(rect.x)},
        {"y", CameraVcaParameterApi::serializeCoordinate(rect.y)},
        {"w", CameraVcaParameterApi::serializeCoordinate(rect.width)},
        {"h", CameraVcaParameterApi::serializeCoordinate(rect.height)},
    };
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::MissingObjectDetection::Rule::MinSize* minSize)
{
    try
    {
        if (!minSize->hasValue())
            return;

        QJsonObject areaLimitation;
        if (!get<QJsonValue>(*jsonRule, "AreaLimitation").isUndefined())
            get(&areaLimitation, *jsonRule, "AreaLimitation");

        areaLimitation["Minimum"] = serializeReToCamera(minSize->value());

        set(jsonRule, "AreaLimitation", areaLimitation);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(minSize, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::MissingObjectDetection::Rule::MaxSize* maxSize)
{
    try
    {
        if (!maxSize->hasValue())
            return;

        QJsonObject areaLimitation;
        if (!get<QJsonValue>(*jsonRule, "AreaLimitation").isUndefined())
            get(&areaLimitation, *jsonRule, "AreaLimitation");

        areaLimitation["Maximum"] = serializeReToCamera(maxSize->value());

        set(jsonRule, "AreaLimitation", areaLimitation);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(maxSize, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::MissingObjectDetection::Rule::Delay* delay)
{
    try
    {
        // If new rule, set default like in web UI.
        if (delay->hasNothing()
            && get<QJsonValue>(*jsonRule, "Duration").isUndefined())
        {
            delay->emplaceValue(300);
        }

        if (!delay->hasValue())
            return;

        set(jsonRule, "Duration", delay->value());
    }
    catch (const std::exception& exception)
    {
        fillReErrors(delay, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::MissingObjectDetection::Rule::RequiresHumanInvolvement* requiresHumanInvolvement)
{
    try
    {
        if (!requiresHumanInvolvement->hasValue())
            return;

        set(jsonRule, "HumanFactor", requiresHumanInvolvement->value());
    }
    catch (const std::exception& exception)
    {
        fillReErrors(requiresHumanInvolvement, exception.what());
    }
}

void serializeReToCamera(QJsonValue* parameters,
    CameraSettings::Vca::MissingObjectDetection* missingObjectDetection)
{
    try
    {
        auto& rules = missingObjectDetection->rules;

        QJsonObject jsonRules;
        if (!get<QJsonValue>(*parameters, "MissingObjectDetection").isUndefined())
        {
            jsonRules = get<QJsonObject>(*parameters, "MissingObjectDetection");
            removeRulesExcept(&jsonRules, getNames(rules));
        }

        for (auto& rule: rules)
        {
            if (auto name = getName(rule))
            {
                QJsonValue jsonRule = jsonRules[*name];
                if (jsonRule.isUndefined() || jsonRule.isNull())
                    jsonRule = QJsonObject{};

                serializeReToCamera(&jsonRule, &rule.region);
                serializeReToCamera(&jsonRule, &rule.minSize);
                serializeReToCamera(&jsonRule, &rule.maxSize);
                serializeReToCamera(&jsonRule, &rule.delay);
                serializeReToCamera(&jsonRule, &rule.requiresHumanInvolvement);

                jsonRules[*name] = jsonRule;
            }
        }

        set(parameters, "MissingObjectDetection", jsonRules);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(missingObjectDetection, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::UnattendedObjectDetection::Rule::MinSize* minSize)
{
    try
    {
        if (!minSize->hasValue())
            return;

        QJsonObject areaLimitation;
        if (!get<QJsonValue>(*jsonRule, "AreaLimitation").isUndefined())
            get(&areaLimitation, *jsonRule, "AreaLimitation");

        areaLimitation["Minimum"] = serializeReToCamera(minSize->value());

        set(jsonRule, "AreaLimitation", areaLimitation);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(minSize, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::UnattendedObjectDetection::Rule::MaxSize* maxSize)
{
    try
    {
        if (!maxSize->hasValue())
            return;

        QJsonObject areaLimitation;
        if (!get<QJsonValue>(*jsonRule, "AreaLimitation").isUndefined())
            get(&areaLimitation, *jsonRule, "AreaLimitation");

        areaLimitation["Maximum"] = serializeReToCamera(maxSize->value());

        set(jsonRule, "AreaLimitation", areaLimitation);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(maxSize, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::UnattendedObjectDetection::Rule::Delay* delay)
{
    try
    {
        // If new rule, set default like in web UI.
        if (delay->hasNothing()
            && get<QJsonValue>(*jsonRule, "Duration").isUndefined())
        {
            delay->emplaceValue(300);
        }

        if (!delay->hasValue())
            return;

        set(jsonRule, "Duration", delay->value());
    }
    catch (const std::exception& exception)
    {
        fillReErrors(delay, exception.what());
    }
}

void serializeReToCamera(QJsonValue* jsonRule,
    CameraSettings::Vca::UnattendedObjectDetection::Rule::RequiresHumanInvolvement* requiresHumanInvolvement)
{
    try
    {
        if (!requiresHumanInvolvement->hasValue())
            return;

        set(jsonRule, "HumanFactor", requiresHumanInvolvement->value());
    }
    catch (const std::exception& exception)
    {
        fillReErrors(requiresHumanInvolvement, exception.what());
    }
}

void serializeReToCamera(QJsonValue* parameters,
    CameraSettings::Vca::UnattendedObjectDetection* unattendedObjectDetection)
{
    try
    {
        auto& rules = unattendedObjectDetection->rules;

        QJsonObject jsonRules;
        if (!get<QJsonValue>(*parameters, "UnattendedObjectDetection").isUndefined())
        {
            jsonRules = get<QJsonObject>(*parameters, "UnattendedObjectDetection");
            removeRulesExcept(&jsonRules, getNames(rules));
        }

        for (auto& rule: rules)
        {
            if (auto name = getName(rule))
            {
                QJsonValue jsonRule = jsonRules[*name];
                if (jsonRule.isUndefined() || jsonRule.isNull())
                    jsonRule = QJsonObject{};

                serializeReToCamera(&jsonRule, &rule.region);
                serializeReToCamera(&jsonRule, &rule.minSize);
                serializeReToCamera(&jsonRule, &rule.maxSize);
                serializeReToCamera(&jsonRule, &rule.delay);
                serializeReToCamera(&jsonRule, &rule.requiresHumanInvolvement);

                jsonRules[*name] = jsonRule;
            }
        }

        set(parameters, "UnattendedObjectDetection", jsonRules);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(unattendedObjectDetection, exception.what());
    }
}

void serializeReToCamera(QJsonValue* parameters,
    CameraSettings::Vca::FaceDetection* faceDetection)
{
    try
    {
        auto& rules = faceDetection->rules;

        QJsonObject jsonRules;
        if (!get<QJsonValue>(*parameters, "FaceDetection").isUndefined())
        {
            jsonRules = get<QJsonObject>(*parameters, "FaceDetection");
            removeRulesExcept(&jsonRules, getNames(rules));
        }

        for (auto& rule: rules)
        {
            if (auto name = getName(rule))
            {
                QJsonValue jsonRule = jsonRules[*name];
                if (jsonRule.isUndefined() || jsonRule.isNull())
                    jsonRule = QJsonObject{};

                serializeReToCamera(&jsonRule, &rule.region);

                jsonRules[*name] = jsonRule;
            }
        }

        set(parameters, "FaceDetection", jsonRules);
    }
    catch (const std::exception& exception)
    {
        fillReErrors(faceDetection, exception.what());
    }
}

void serializeReToCamera(QJsonValue* parameters, CameraSettings::Vca* vca)
{
    if (auto& crowdDetection = vca->crowdDetection)
        serializeReToCamera(parameters, &*crowdDetection);
    if (auto& loiteringDetection = vca->loiteringDetection)
        serializeReToCamera(parameters, &*loiteringDetection);
    if (auto& intrusionDetection = vca->intrusionDetection)
        serializeReToCamera(parameters, &*intrusionDetection);
    if (auto& lineCrossingDetection = vca->lineCrossingDetection)
        serializeReToCamera(parameters, &*lineCrossingDetection);
    if (auto& missingObjectDetection = vca->missingObjectDetection)
        serializeReToCamera(parameters, &*missingObjectDetection);
    if (auto& unattendedObjectDetection = vca->unattendedObjectDetection)
        serializeReToCamera(parameters, &*unattendedObjectDetection);
    if (auto& faceDetection = vca->faceDetection)
        serializeReToCamera(parameters, &*faceDetection);
}


void storeReToCamera(CameraVcaParameterApi* api, CameraSettings::Vca* vca)
{
    try
    {
        auto parameters = api->fetch("Config/RE").get();

        serializeReToCamera(&parameters, vca);

        api->store("Config/RE", parameters).get();
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

    CameraVcaParameterApi api(cameraUrl);

    storeAeToCamera(&api, vca);
    storeReToCamera(&api, vca);

    api.reloadConfig().get();
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

        if (auto& crowdDetection = vca->crowdDetection)
        {
            auto& rules = crowdDetection->rules;
            for (std::size_t i = 0; i < rules.size(); ++i)
            {
                auto& rule = rules[i];

                repeatedVisit(i, &rule.region);
                repeatedVisit(i, &rule.sizeThreshold);
                repeatedVisit(i, &rule.enterDelay);
                repeatedVisit(i, &rule.exitDelay);
            }
        }

        if (auto& loiteringDetection = vca->loiteringDetection)
        {
            auto& rules = loiteringDetection->rules;
            for (std::size_t i = 0; i < rules.size(); ++i)
            {
                auto& rule = rules[i];

                repeatedVisit(i, &rule.region);
                repeatedVisit(i, &rule.delay);
            }
        }

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

        if (auto& lineCrossingDetection = vca->lineCrossingDetection)
        {
            auto& rules = lineCrossingDetection->rules;
            for (std::size_t i = 0; i < rules.size(); ++i)
            {
                auto& rule = rules[i];

                repeatedVisit(i, &rule.line);
            }
        }

        if (auto& missingObjectDetection = vca->missingObjectDetection)
        {
            auto& rules = missingObjectDetection->rules;
            for (std::size_t i = 0; i < rules.size(); ++i)
            {
                auto& rule = rules[i];

                repeatedVisit(i, &rule.region);
                repeatedVisit(i, &rule.minSize);
                repeatedVisit(i, &rule.maxSize);
                repeatedVisit(i, &rule.delay);
                repeatedVisit(i, &rule.requiresHumanInvolvement);
            }
        }

        if (auto& unattendedObjectDetection = vca->unattendedObjectDetection)
        {
            auto& rules = unattendedObjectDetection->rules;
            for (std::size_t i = 0; i < rules.size(); ++i)
            {
                auto& rule = rules[i];

                repeatedVisit(i, &rule.region);
                repeatedVisit(i, &rule.minSize);
                repeatedVisit(i, &rule.maxSize);
                repeatedVisit(i, &rule.delay);
                repeatedVisit(i, &rule.requiresHumanInvolvement);
            }
        }

        if (auto& faceDetection = vca->faceDetection)
        {
            auto& rules = faceDetection->rules;
            for (std::size_t i = 0; i < rules.size(); ++i)
            {
                auto& rule = rules[i];

                repeatedVisit(i, &rule.region);
            }
        }
    }
}


void parseEntryFromServer(
    CameraSettings::Vca::IntrusionDetection::Rule::Inverted* entry, const QString& serializedValue)
{
    if (serializedValue == "")
        entry->emplaceNothing();
    else if (serializedValue == "false")
        entry->emplaceValue(false);
    else if (serializedValue == "true")
        entry->emplaceValue(true);
    else
        throw Exception("Failed to parse boolean");
}

void parseEntryFromServer(
    CameraSettings::Vca::MissingObjectDetection::Rule::RequiresHumanInvolvement* entry,
    const QString& serializedValue)
{
    if (serializedValue == "")
        entry->emplaceNothing();
    else if (serializedValue == "false")
        entry->emplaceValue(false);
    else if (serializedValue == "true")
        entry->emplaceValue(true);
    else
        throw Exception("Failed to parse boolean");
}

void parseEntryFromServer(
    CameraSettings::Vca::UnattendedObjectDetection::Rule::RequiresHumanInvolvement* entry,
    const QString& serializedValue)
{
    if (serializedValue == "")
        entry->emplaceNothing();
    else if (serializedValue == "false")
        entry->emplaceValue(false);
    else if (serializedValue == "true")
        entry->emplaceValue(true);
    else
        throw Exception("Failed to parse boolean");
}

void parseEntryFromServer(
    CameraSettings::Entry<bool>* entry, const QString& serializedValue)
{
    if (serializedValue == "false")
        entry->emplaceValue(false);
    else if (serializedValue == "true")
        entry->emplaceValue(true);
    else
        throw Exception("Failed to parse boolean");
}

void parseEntryFromServer(
    CameraSettings::Vca::Sensitivity* entry, const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 1, 10));
}

void parseEntryFromServer(
    CameraSettings::Vca::Installation::Height* entry, const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 2000));
}

void parseEntryFromServer(
    CameraSettings::Vca::Installation::TiltAngle* entry, const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 179));
}

void parseEntryFromServer(
    CameraSettings::Vca::Installation::RollAngle* entry, const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), -74, +74));
}

void parseEntryFromServer(
    CameraSettings::Vca::CrowdDetection::Rule::SizeThreshold* entry, const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 20));
}

void parseEntryFromServer(
    CameraSettings::Vca::CrowdDetection::Rule::EnterDelay* entry, const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
}

void parseEntryFromServer(
    CameraSettings::Vca::CrowdDetection::Rule::ExitDelay* entry, const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
}

void parseEntryFromServer(
    CameraSettings::Vca::LoiteringDetection::Rule::Delay* entry, const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
}

void parseEntryFromServer(
    CameraSettings::Vca::MissingObjectDetection::Rule::Delay* entry, const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
}

void parseEntryFromServer(
    CameraSettings::Vca::UnattendedObjectDetection::Rule::Delay* entry, const QString& serializedValue)
{
    if (serializedValue.isEmpty())
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
}

void parseEntryFromServer(CameraSettings::Entry<int>* entry, const QString& serializedValue)
{
    entry->emplaceValue(toInt(serializedValue));
}

bool parseFromServer(NamedPointSequence* points, const QJsonValue& json)
{
    const auto figure = get<QJsonValue>(json, "figure");
    if (figure.isNull())
        return false;

    points->name = get<QString>(json, "label");
    if (points->name.isEmpty())
        throw Exception("Empty name");

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
    CameraSettings::Entry<NamedPolygon>* entry, const QString& serializedValue)
{
    const auto json = parseJson(serializedValue.toUtf8());

    NamedPolygon polygon;
    if (!parseFromServer(&polygon, json))
    {
        entry->emplaceNothing();
        return;
    }

    entry->emplaceValue(std::move(polygon));
}

void parseFromServer(NamedLine::Direction* direction, const QString& serializedValue)
{
    if (serializedValue == "absent")
        *direction = NamedLine::Direction::any;
    else if (serializedValue == "right")
        *direction = NamedLine::Direction::leftToRight;
    else if (serializedValue == "left")
        *direction = NamedLine::Direction::rightToLeft;
    else
        throw Exception("Unknown LineFigure direction: %1", serializedValue);
}

void parseEntryFromServer(
    CameraSettings::Entry<NamedLine>* entry, const QString& serializedValue)
{
    const auto json = parseJson(serializedValue.toUtf8());

    NamedLine line;
    if (!parseFromServer(&line, json))
    {
        entry->emplaceNothing();
        return;
    }

    parseFromServer(&line.direction, get<QString>(json, "figure", "direction"));

    entry->emplaceValue(std::move(line));
}

void parseEntryFromServer(
    CameraSettings::Entry<Rect>* entry, const QString& serializedValue)
{
    const auto json = parseJson(serializedValue.toUtf8());

    const auto figure = get<QJsonValue>(json, "figure");
    if (figure.isNull())
    {
        entry->emplaceNothing();
        return;
    }

    const auto jsonPoints = get<QJsonArray>("$.figure", figure, "points");
    if (jsonPoints.count() != 2)
        throw Exception("Unexpected BoxFigure point count: %1", jsonPoints.count());

    std::vector<Point> points;
    for (int i = 0; i < jsonPoints.count(); ++i)
    {
        const auto point = get<QJsonArray>("$.figure.points", jsonPoints, i);
        const QString path = NX_FMT("$.figure.points[%1]", i);
        points.emplace_back(
            get<double>(path, point, 0),
            get<double>(path, point, 1));
    }

    Rect rect;
    rect.x = points[0].x;
    rect.y = points[0].y;
    rect.width = points[1].x - points[0].x;
    rect.height = points[1].y - points[0].y;

    entry->emplaceValue(rect);
}


std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::IntrusionDetection::Rule::Inverted& entry)
{
    if (!entry.hasValue())
        return "";

    return entry.value() ? "true" : "false";
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::MissingObjectDetection::Rule::RequiresHumanInvolvement& entry)
{
    if (!entry.hasValue())
        return "";

    return entry.value() ? "true" : "false";
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::UnattendedObjectDetection::Rule::RequiresHumanInvolvement& entry)
{
    if (!entry.hasValue())
        return "";

    return entry.value() ? "true" : "false";
}

std::optional<QString> serializeEntryToServer(const CameraSettings::Entry<bool>& entry)
{
    if (!entry.hasValue())
        return std::nullopt;

    return entry.value() ? "true" : "false";
}

std::optional<QString> serializeEntryToServer(const CameraSettings::Vca::Sensitivity& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::Installation::Height& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::Installation::TiltAngle& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::Installation::RollAngle& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::CrowdDetection::Rule::SizeThreshold& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::CrowdDetection::Rule::EnterDelay& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::CrowdDetection::Rule::ExitDelay& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::LoiteringDetection::Rule::Delay& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::MissingObjectDetection::Rule::Delay& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> serializeEntryToServer(
    const CameraSettings::Vca::UnattendedObjectDetection::Rule::Delay& entry)
{
    if (!entry.hasValue())
        return "";

    return QString::number(entry.value());
}

std::optional<QString> serializeEntryToServer(const CameraSettings::Entry<int>& entry)
{
    if (!entry.hasValue())
        return std::nullopt;

    return QString::number(entry.value());
}

QJsonObject serializeToServer(const NamedPointSequence* points)
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

std::optional<QString> serializeEntryToServer(const CameraSettings::Entry<NamedPolygon>& entry)
{
    const NamedPolygon* polygon = nullptr;
    if (entry.hasValue())
        polygon = &entry.value();

    auto json = serializeToServer(polygon);

    return serializeJson(json);
}

QString serializeToServer(NamedLine::Direction direction)
{
    switch (direction)
    {
        case NamedLine::Direction::any:
            return "absent";
        case NamedLine::Direction::leftToRight:
            return "right";
        case NamedLine::Direction::rightToLeft:
            return "left";
        default:
            NX_ASSERT(false, "Unknown NamedLine::Direction value: %1", (int) direction);
            return "";
    }
}

std::optional<QString> serializeEntryToServer(const CameraSettings::Entry<NamedLine>& entry)
{
    const NamedLine* line = nullptr;
    if (entry.hasValue())
        line = &entry.value();

    auto json = serializeToServer(line);

    if (line)
        set(&json, "figure", "direction", serializeToServer(line->direction));

    return serializeJson(json);
}

std::optional<QString> serializeEntryToServer(const CameraSettings::Entry<Rect>& entry)
{
    if (!entry.hasValue())
    {
        return serializeJson(QJsonObject{
            {"label", ""},
            {"figure", QJsonValue::Null},
        });
    }

    const auto& rect = entry.value();
    return serializeJson(QJsonObject{
        {"label", ""},
        {"figure", QJsonObject{
            {"points", QJsonArray{
                QJsonArray{rect.x, rect.y},
                QJsonArray{
                    rect.x + rect.width,
                    rect.y + rect.height,
                },
            }},
        }},
    });
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

QJsonValue getVcaCrowdDetectionModelForManifest()
{
    using Rule = CameraSettings::Vca::CrowdDetection::Rule;
    return QJsonObject{
        {"name", "Vca.CrowdDetection"},
        {"type", "Section"},
        {"caption", "Crowd Detection"},
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
                            {"caption", "Region"},
                            {"minPoints", 3},
                            {"maxPoints", 20}, // Defined in user guide.
                        },
                        QJsonObject{
                            {"name", Rule::SizeThreshold::name},
                            // Should really be SpinBox, but need empty state to work around camera not
                            // returning settings for disabled functionality.
                            {"type", "TextField"},
                            {"caption", "Size threshold"},
                            {"description", "At least this many people must be in the area to be considered a crowd"},
                        },
                        QJsonObject{
                            {"name", Rule::EnterDelay::name},
                            // Should really be SpinBox, but need empty state to work around camera not
                            // returning settings for disabled functionality.
                            {"type", "TextField"},
                            {"caption", "Entrance delay (s)"},
                            {"description", "The event is only generated if the person stays in the area for at least this long"},
                        },
                        QJsonObject{
                            {"name", Rule::ExitDelay::name},
                            // Should really be SpinBox, but need empty state to work around camera not
                            // returning settings for disabled functionality.
                            {"type", "TextField"},
                            {"caption", "Exit delay (s)"},
                            {"description", "The event is only generated if the person stays out of the area for at least this long"},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonValue getVcaLoiteringDetectionModelForManifest()
{
    using Rule = CameraSettings::Vca::LoiteringDetection::Rule;
    return QJsonObject{
        {"name", "Vca.LoiteringDetection"},
        {"type", "Section"},
        {"caption", "Loitering Detection"},
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
                            {"caption", "Region"},
                            {"minPoints", 3},
                            {"maxPoints", 20}, // Defined in user guide.
                        },
                        QJsonObject{
                            {"name", Rule::Delay::name},
                            // Should really be SpinBox, but need empty state to work around camera not
                            // returning settings for disabled functionality.
                            {"type", "TextField"},
                            {"caption", "Delay (s)"},
                            {"description", "The event is only generated if a person stays in the area for at least this long"},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonValue getVcaIntrusionDetectionModelForManifest()
{
    using Rule = CameraSettings::Vca::IntrusionDetection::Rule;
    return QJsonObject{
        {"name", "Vca.IntrusionDetection"},
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
                            {"caption", "Region"},
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

QJsonValue getVcaLineCrossingDetectionModelForManifest()
{
    using Rule = CameraSettings::Vca::LineCrossingDetection::Rule;
    return QJsonObject{
        {"name", "Vca.LineCrossingDetection"},
        {"type", "Section"},
        {"caption", "Line Crossing Detection"},
        {"items", QJsonArray{
            QJsonObject{
                {"type", "Repeater"},
                {"startIndex", 1},
                {"count", (int) kMaxDetectionRuleCount},
                {"template", QJsonObject{
                    {"type", "GroupBox"},
                    {"caption", "Rule #"},
                    {"filledCheckItems", QJsonArray{Rule::Line::name}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", Rule::Line::name},
                            {"type", "LineFigure"},
                            {"caption", "Line"},
                            {"minPoints", 3},
                            {"maxPoints", 3},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonValue getVcaMissingObjectDetectionModelForManifest()
{
    using Rule = CameraSettings::Vca::MissingObjectDetection::Rule;
    return QJsonObject{
        {"name", "Vca.MissingObjectDetection"},
        {"type", "Section"},
        {"caption", "Missing Object Detection"},
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
                            {"caption", "Region"},
                            {"minPoints", 3},
                            {"maxPoints", 20}, // Defined in user guide.
                        },
                        // THe following two BoxFigures should really be single
                        // ObjectSizeConstraints, but need empty state to work around camera not
                        // returning settings for disabled functionality.
                        QJsonObject{
                            {"name", Rule::MinSize::name},
                            {"type", "BoxFigure"},
                            {"caption", "Minimum size"},
                        },
                        QJsonObject{
                            {"name", Rule::MaxSize::name},
                            {"type", "BoxFigure"},
                            {"caption", "Maximum size"},
                        },
                        QJsonObject{
                            {"name", Rule::Delay::name},
                            // Should really be SpinBox, but need empty state to work around camera not
                            // returning settings for disabled functionality.
                            {"type", "TextField"},
                            {"caption", "Delay (s)"},
                            {"description", "The event is only generated if an object is missing for at least this long"},
                        },
                        QJsonObject{
                            {"name", Rule::RequiresHumanInvolvement::name},
                            {"type", "ComboBox"},
                            {"caption", "Human involvement"},
                            {"description", "Whether a human is required to walk by an object prior to it going missing"},
                            {"range", QJsonArray{
                                // Need empty state to work around camera not returning settings
                                // for disabled functionality.
                                "",
                                "false",
                                "true",
                            }},
                            {"itemCaptions", QJsonObject{
                                {"", ""},
                                {"false", "Not required"},
                                {"true", "Required"},
                            }},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonValue getVcaUnattendedObjectDetectionModelForManifest()
{
    using Rule = CameraSettings::Vca::UnattendedObjectDetection::Rule;
    return QJsonObject{
        {"name", "Vca.UnattendedObjectDetection"},
        {"type", "Section"},
        {"caption", "Unattended Object Detection"},
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
                            {"caption", "Region"},
                            {"minPoints", 3},
                            {"maxPoints", 20}, // Defined in user guide.
                        },
                        // THe following two BoxFigures should really be single
                        // ObjectSizeConstraints, but need empty state to work around camera not
                        // returning settings for disabled functionality.
                        QJsonObject{
                            {"name", Rule::MinSize::name},
                            {"type", "BoxFigure"},
                            {"caption", "Minimum size"},
                        },
                        QJsonObject{
                            {"name", Rule::MaxSize::name},
                            {"type", "BoxFigure"},
                            {"caption", "Maximum size"},
                        },
                        QJsonObject{
                            {"name", Rule::Delay::name},
                            // Should really be SpinBox, but need empty state to work around camera not
                            // returning settings for disabled functionality.
                            {"type", "TextField"},
                            {"caption", "Delay (s)"},
                            {"description", "The event is only generated if an object is unattended for at least this long"},
                        },
                        QJsonObject{
                            {"name", Rule::RequiresHumanInvolvement::name},
                            {"type", "ComboBox"},
                            {"caption", "Human involvement"},
                            {"description", "Whether a human is required to walk by an object prior to it becoming unattended"},
                            {"range", QJsonArray{
                                // Need empty state to work around camera not returning settings
                                // for disabled functionality.
                                "",
                                "false",
                                "true",
                            }},
                            {"itemCaptions", QJsonObject{
                                {"", ""},
                                {"false", "Not required"},
                                {"true", "Required"},
                            }},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonValue getVcaFaceDetectionModelForManifest()
{
    using Rule = CameraSettings::Vca::FaceDetection::Rule;
    return QJsonObject{
        {"name", "Vca.FaceDetection"},
        {"type", "Section"},
        {"caption", "Face Detection"},
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
                            {"caption", "Region"},
                            {"minPoints", 3},
                            {"maxPoints", 20}, // Defined in user guide.
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

                if (features.crowdDetection)
                    sections.push_back(getVcaCrowdDetectionModelForManifest());
                if (features.loiteringDetection)
                    sections.push_back(getVcaLoiteringDetectionModelForManifest());
                if (features.intrusionDetection)
                    sections.push_back(getVcaIntrusionDetectionModelForManifest());
                if (features.lineCrossingDetection)
                    sections.push_back(getVcaLineCrossingDetectionModelForManifest());
                if (features.missingObjectDetection)
                    sections.push_back(getVcaMissingObjectDetectionModelForManifest());
                if (features.unattendedObjectDetection)
                    sections.push_back(getVcaUnattendedObjectDetectionModelForManifest());
                if (features.faceDetection)
                    sections.push_back(getVcaFaceDetectionModelForManifest());

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

        if (features.vca->crowdDetection)
        {
            auto& crowdDetection = vca->crowdDetection.emplace();
            crowdDetection.rules.resize(kMaxDetectionRuleCount);
        }
        if (features.vca->loiteringDetection)
        {
            auto& loiteringDetection = vca->loiteringDetection.emplace();
            loiteringDetection.rules.resize(kMaxDetectionRuleCount);
        }
        if (features.vca->intrusionDetection)
        {
            auto& intrusionDetection = vca->intrusionDetection.emplace();
            intrusionDetection.rules.resize(kMaxDetectionRuleCount);
        }
        if (features.vca->lineCrossingDetection)
        {
            auto& lineCrossingDetection = vca->lineCrossingDetection.emplace();
            lineCrossingDetection.rules.resize(kMaxDetectionRuleCount);
        }
        if (features.vca->missingObjectDetection)
        {
            auto& missingObjectDetection = vca->missingObjectDetection.emplace();
            missingObjectDetection.rules.resize(kMaxDetectionRuleCount);
        }
        if (features.vca->unattendedObjectDetection)
        {
            auto& unattendedObjectDetection = vca->unattendedObjectDetection.emplace();
            unattendedObjectDetection.rules.resize(kMaxDetectionRuleCount);
        }
        if (features.vca->faceDetection)
        {
            auto& faceDetection = vca->faceDetection.emplace();
            faceDetection.rules.resize(kMaxDetectionRuleCount);
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
                const char* serializedValue = values.value(name.toStdString().data());
                if (!serializedValue)
                {
                    entry->emplaceNothing();
                    return;
                }

                parseEntryFromServer(entry, serializedValue);
            }
            catch (const std::exception& exception)
            {
                entry->emplaceErrorMessage(exception.what());
            }
        });
}

Ptr<StringMap> CameraSettings::serializeToServer()
{
    auto values = makePtr<StringMap>();

    enumerateEntries(this,
        [&](const QString& name, auto* entry)
        {
            try
            {
                if (const auto serializedValue = serializeEntryToServer(*entry))
                    values->setItem(name.toStdString(), serializedValue->toStdString());
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
