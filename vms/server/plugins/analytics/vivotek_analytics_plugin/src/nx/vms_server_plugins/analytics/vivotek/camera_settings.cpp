#include "camera_settings.h"

#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/helpers/settings_response.h>
#include <nx/utils/log/log_message.h>

#include <QtCore/QString>
#include <QtCore/QUrlQuery>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include "http_client.h"
#include "camera_module_api.h"
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


//template <typename Visitor>
//void enumerateAeEntries(CameraSettings::Vca* vca, Visitor visit)
//{
//    visit(&vca->sensitivity, "Sensitivity");
//    visit(&vca->installation.height, "CamHeight");
//    visit(&vca->installation.tiltAngle, "TiltAngle");
//    visit(&vca->installation.rollAngle, "RollAngle");
//}
//
//
//template <typename Value>
//void fillReErrors(CameraSettings::Entry<Value>* entry,
//    const QString& message)
//{
//    entry->emplaceErrorMessage(message);
//}
//
//void fillReErrors(CameraSettings::Vca::Installation* installation,
//    const QString& message)
//{
//    for (auto& exclusion: installation->exclusions)
//    {
//        fillReErrors(&exclusion.region, message);
//    }
//}
//
//void fillReErrors(CameraSettings::Vca::CrowdDetection* crowdDetection,
//    const QString& message)
//{
//    for (auto& rule: crowdDetection->rules)
//    {
//        fillReErrors(&rule.region, message);
//        fillReErrors(&rule.sizeThreshold, message);
//        fillReErrors(&rule.enterDelay, message);
//        fillReErrors(&rule.exitDelay, message);
//    }
//}
//
//void fillReErrors(CameraSettings::Vca::LoiteringDetection* loiteringDetection,
//    const QString& message)
//{
//    for (auto& rule: loiteringDetection->rules)
//    {
//        fillReErrors(&rule.region, message);
//        fillReErrors(&rule.delay, message);
//    }
//}
//
//void fillReErrors(CameraSettings::Vca::IntrusionDetection* intrusionDetection,
//    const QString& message)
//{
//    for (auto& rule: intrusionDetection->rules)
//    {
//        fillReErrors(&rule.region, message);
//        fillReErrors(&rule.inverted, message);
//    }
//}
//
//void fillReErrors(CameraSettings::Vca::LineCrossingDetection* lineCrossingDetection,
//    const QString& message)
//{
//    for (auto& rule: lineCrossingDetection->rules)
//    {
//        fillReErrors(&rule.line, message);
//    }
//}
//
//void fillReErrors(CameraSettings::Vca::MissingObjectDetection* missingObjectDetection,
//    const QString& message)
//{
//    for (auto& rule: missingObjectDetection->rules)
//    {
//        fillReErrors(&rule.region, message);
//        fillReErrors(&rule.minSize, message);
//        fillReErrors(&rule.maxSize, message);
//        fillReErrors(&rule.delay, message);
//        fillReErrors(&rule.requiresHumanInvolvement, message);
//    }
//}
//
//void fillReErrors(CameraSettings::Vca::UnattendedObjectDetection* unattendedObjectDetection,
//    const QString& message)
//{
//    for (auto& rule: unattendedObjectDetection->rules)
//    {
//        fillReErrors(&rule.region, message);
//        fillReErrors(&rule.minSize, message);
//        fillReErrors(&rule.maxSize, message);
//        fillReErrors(&rule.delay, message);
//        fillReErrors(&rule.requiresHumanInvolvement, message);
//    }
//}
//
//void fillReErrors(CameraSettings::Vca::FaceDetection* faceDetection,
//    const QString& message)
//{
//    for (auto& rule: faceDetection->rules)
//    {
//        fillReErrors(&rule.region, message);
//    }
//}
//
//void fillReErrors(CameraSettings::Vca::RunningDetection* runningDetection,
//    const QString& message)
//{
//    for (auto& rule: runningDetection->rules)
//    {
//        fillReErrors(&rule.name, message);
//        fillReErrors(&rule.minCount, message);
//        fillReErrors(&rule.minSpeed, message);
//        fillReErrors(&rule.delay, message);
//    }
//}
//
//void fillReErrors(CameraSettings::Vca* vca, const QString& message)
//{
//    fillReErrors(&vca->installation, message);
//    if (auto& crowdDetection = vca->crowdDetection)
//        fillReErrors(&*crowdDetection, message);
//    if (auto& loiteringDetection = vca->loiteringDetection)
//        fillReErrors(&*loiteringDetection, message);
//    if (auto& intrusionDetection = vca->intrusionDetection)
//        fillReErrors(&*intrusionDetection, message);
//    if (auto& lineCrossingDetection = vca->lineCrossingDetection)
//        fillReErrors(&*lineCrossingDetection, message);
//    if (auto& unattendedObjectDetection = vca->unattendedObjectDetection)
//        fillReErrors(&*unattendedObjectDetection, message);
//    if (auto& faceDetection = vca->faceDetection)
//        fillReErrors(&*faceDetection, message);
//    if (auto& runningDetection = vca->runningDetection)
//        fillReErrors(&*runningDetection, message);
//}
//
//
//void fetchFromCamera(CameraSettings::Vca::Enabled* enabled, const Url& cameraUrl)
//{
//    try
//    {
//        const auto moduleInfo = VadpModuleInfo::fetchFrom(cameraUrl, "VCA");
//        enabled->emplaceValue(moduleInfo.enabled);
//    }
//    catch (const std::exception& exception)
//    {
//        enabled->emplaceErrorMessage(exception.what());
//    }
//}
//
//
//template <typename... Keys>
//void parseAeFromCamera(CameraSettings::Vca::Installation::Height* entry,
//    const QJsonValue& parameters, const Keys&... keys)
//{
//    using Value = CameraSettings::Vca::Installation::Height::Value;
//    // API uses millimeters, while web UI uses centimeters.
//    entry->emplaceValue(get<Value>(parameters, keys...) / 10);
//}
//
//template <typename Value, typename... Keys>
//void parseAeFromCamera(CameraSettings::Entry<Value>* entry,
//    const QJsonValue& parameters, const Keys&... keys)
//{
//    entry->emplaceValue(get<Value>(parameters, keys...));
//}
//
//
//void fetchAeFromCamera(CameraSettings::Vca* vca, CameraVcaParameterApi* api)
//{
//    try
//    {
//        const auto parameters = api->fetch("Config/AE").get();
//
//        enumerateAeEntries(vca,
//            [&](auto* entry, const auto&... keys)
//            {
//                try
//                {
//                    parseAeFromCamera(entry, parameters, keys...);
//                }
//                catch (const std::exception& exception)
//                {
//                    entry->emplaceErrorMessage(exception.what());
//                }
//            });
//    }
//    catch (const std::exception& exception)
//    {
//        const QString message = exception.what();
//        enumerateAeEntries(vca,
//            [&](auto* entry, auto&&...) { entry->emplaceErrorMessage(message); });
//    }
//}
//
//void parseReFromCamera(CameraSettings::Entry<NamedPolygon>* region,
//    const QJsonValue& rule, const QString& ruleName, const QString& path)
//{
//    try
//    {
//        auto& value = region->emplaceValue();
//
//        value.name = ruleName;
//
//        // That last 0 is intentional. For some reason, camera returns each region as an array of
//        // a single array of points.
//        const auto field = get<QJsonArray>(path, rule, "Field", 0);
//        for (int i = 0; i < field.size(); ++i)
//        {
//            value.push_back(CameraVcaParameterApi::parsePoint(
//                field[i], NX_FMT("%1.Field[%2]", path, i)));
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        region->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::Installation* installation,
//    const QJsonValue& parameters)
//{
//    try
//    {
//        auto& exclusions = installation->exclusions;
//        for (auto& exclusion: exclusions)
//            exclusion.region.emplaceNothing();
//
//        if (get<QJsonValue>(parameters, "ExclusiveArea").isUndefined())
//            return;
//
//        auto field = get<QJsonArray>(parameters, "ExclusiveArea", "Field");
//        while ((std::size_t) field.count() > exclusions.size())
//            field.pop_back();
//        const QString path = "$.ExclusiveArea.Field";
//        for (int i = 0; i < field.count(); ++i)
//        {
//            const auto subField = get<QJsonArray>(path, field, i);
//
//            auto& region = exclusions[i].region.emplaceValue();
//
//            for (int j = 0; j < subField.count(); ++j)
//            {
//                region.push_back(CameraVcaParameterApi::parsePoint(
//                    subField[j], NX_FMT("%1[%2][%3]", path, i, j)));
//            }
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(installation, exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::CrowdDetection::Rule::SizeThreshold* sizeThreshold,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        sizeThreshold->emplaceValue(get<int>(path, rule, "PeopleNumber"));
//    }
//    catch (const std::exception& exception)
//    {
//        sizeThreshold->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::CrowdDetection::Rule::EnterDelay* enterDelay,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        enterDelay->emplaceValue(get<int>(path, rule, "EnterDelay"));
//    }
//    catch (const std::exception& exception)
//    {
//        enterDelay->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::CrowdDetection::Rule::ExitDelay* exitDelay,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        exitDelay->emplaceValue(get<int>(path, rule, "LeaveDelay"));
//    }
//    catch (const std::exception& exception)
//    {
//        exitDelay->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::CrowdDetection* crowdDetection,
//    const QJsonValue& parameters)
//{
//    try
//    {
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(parameters, "CrowdDetection").isUndefined())
//            get(&jsonRules, parameters, "CrowdDetection");
//
//        const auto ruleNames = jsonRules.keys();
//
//        auto& rules = crowdDetection->rules;
//        for (std::size_t i = 0; i < rules.size(); ++i)
//        {
//            auto& rule = rules[i];
//            if (i >= (std::size_t) ruleNames.size())
//            {
//                rule.region.emplaceNothing();
//                rule.sizeThreshold.emplaceNothing();
//                rule.enterDelay.emplaceNothing();
//                rule.exitDelay.emplaceNothing();
//                continue;
//            }
//
//            const auto& ruleName = ruleNames[i];
//            const auto& jsonRule = jsonRules[ruleName];
//
//            const QString path = NX_FMT("$.CrowdDetection.%1", ruleName);
//
//            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
//            parseReFromCamera(&rule.sizeThreshold, jsonRule, path);
//            parseReFromCamera(&rule.enterDelay, jsonRule, path);
//            parseReFromCamera(&rule.exitDelay, jsonRule, path);
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(crowdDetection, exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::LoiteringDetection::Rule::Delay* delay,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        delay->emplaceValue(get<int>(path, rule, "StayTime"));
//    }
//    catch (const std::exception& exception)
//    {
//        delay->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::LoiteringDetection* loiteringDetection,
//    const QJsonValue& parameters)
//{
//    try
//    {
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(parameters, "LoiteringDetection").isUndefined())
//            get(&jsonRules, parameters, "LoiteringDetection");
//
//        const auto ruleNames = jsonRules.keys();
//
//        auto& rules = loiteringDetection->rules;
//        for (std::size_t i = 0; i < rules.size(); ++i)
//        {
//            auto& rule = rules[i];
//            if (i >= (std::size_t) ruleNames.size())
//            {
//                rule.region.emplaceNothing();
//                rule.delay.emplaceNothing();
//                continue;
//            }
//
//            const auto& ruleName = ruleNames[i];
//            const auto& jsonRule = jsonRules[ruleName];
//
//            const QString path = NX_FMT("$.LoiteringDetection.%1", ruleName);
//
//            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
//            parseReFromCamera(&rule.delay, jsonRule, path);
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(loiteringDetection, exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::IntrusionDetection::Rule::Inverted* inverted,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        const auto walkingDirection = get<QString>(path, rule, "WalkingDirection");
//        if (walkingDirection == "OutToIn")
//            inverted->emplaceValue(false);
//        else if (walkingDirection == "InToOut")
//            inverted->emplaceValue(true);
//        else
//        {
//            throw Exception("%1.WalkingDirection has unexpected value: %2",
//                path, walkingDirection);
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        inverted->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::IntrusionDetection* intrusionDetection,
//    const QJsonValue& parameters)
//{
//    try
//    {
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(parameters, "IntrusionDetection").isUndefined())
//            get(&jsonRules, parameters, "IntrusionDetection");
//
//        const auto ruleNames = jsonRules.keys();
//
//        auto& rules = intrusionDetection->rules;
//        for (std::size_t i = 0; i < rules.size(); ++i)
//        {
//            auto& rule = rules[i];
//            if (i >= (std::size_t) ruleNames.size())
//            {
//                rule.region.emplaceNothing();
//                rule.inverted.emplaceNothing();
//                continue;
//            }
//
//            const auto& ruleName = ruleNames[i];
//            const auto& jsonRule = jsonRules[ruleName];
//
//            const QString path = NX_FMT("$.IntrusionDetection.%1", ruleName);
//
//            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
//            parseReFromCamera(&rule.inverted, jsonRule, path);
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(intrusionDetection, exception.what());
//    }
//}
//
//void parseReFromCamera(NamedLine::Direction* direction,
//    const QJsonValue& rule, const QString& path)
//{
//    const auto nativeDirection = get<QString>(path, rule, "Direction");
//    if (nativeDirection == "Any")
//        *direction = NamedLine::Direction::any;
//    else if (nativeDirection == "Out")
//        *direction = NamedLine::Direction::leftToRight;
//    else if (nativeDirection == "In")
//        *direction = NamedLine::Direction::rightToLeft;
//    else
//        throw Exception("Unexpected value of %1.Direction: %2", path, nativeDirection);
//}
//
//void parseReFromCamera(CameraSettings::Entry<NamedLine>* line,
//    const QJsonValue& rule, const QString& ruleName, const QString& path)
//{
//    try
//    {
//        auto& value = line->emplaceValue();
//
//        value.name = ruleName;
//
//        // That last 0 is intentional. For some reason, camera returns each line as an array of
//        // a single array of points.
//        const auto jsonLine = get<QJsonArray>(path, rule, "Line", 0);
//        for (int i = 0; i < jsonLine.size(); ++i)
//        {
//            value.push_back(CameraVcaParameterApi::parsePoint(
//                jsonLine[i], NX_FMT("%1.Line[%2]", path, i)));
//        }
//
//        parseReFromCamera(&value.direction, rule, path);
//    }
//    catch (const std::exception& exception)
//    {
//        line->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::LineCrossingDetection* lineCrossingDetection,
//    const QJsonValue& parameters)
//{
//    try
//    {
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(parameters, "LineCrossingDetection").isUndefined())
//            get(&jsonRules, parameters, "LineCrossingDetection");
//
//        const auto ruleNames = jsonRules.keys();
//
//        auto& rules = lineCrossingDetection->rules;
//        for (std::size_t i = 0; i < rules.size(); ++i)
//        {
//            auto& rule = rules[i];
//            if (i >= (std::size_t) ruleNames.size())
//            {
//                rule.line.emplaceNothing();
//                continue;
//            }
//
//            const auto& ruleName = ruleNames[i];
//            const auto& jsonRule = jsonRules[ruleName];
//
//            const QString path = NX_FMT("$.LineCrossingDetection.%1", ruleName);
//
//            parseReFromCamera(&rule.line, jsonRule, ruleName, path);
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(lineCrossingDetection, exception.what());
//    }
//}
//
//void parseReFromCamera(Rect* rect, const QJsonObject& json, const QString& path)
//{
//    const auto parseCoordinate =
//        [&](const QString& key)
//        {
//            return CameraVcaParameterApi::parseCoordinate(
//                get<QJsonValue>(path, json, key),
//                NX_FMT("%1.%2", path, key));
//        };
//
//    rect->x = parseCoordinate("x");
//    rect->y = parseCoordinate("y");
//    rect->width = parseCoordinate("w");
//    rect->height = parseCoordinate("h");
//}
//
//void parseReFromCamera(CameraSettings::Vca::MissingObjectDetection::Rule::MinSize* minSize,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        parseReFromCamera(&minSize->emplaceValue(),
//            get<QJsonObject>(path, rule, "AreaLimitation", "Minimum"),
//            NX_FMT("%1.AreaLimitation.Minimum", path));
//    }
//    catch (const std::exception& exception)
//    {
//        minSize->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::MissingObjectDetection::Rule::MaxSize* maxSize,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        parseReFromCamera(&maxSize->emplaceValue(),
//            get<QJsonObject>(path, rule, "AreaLimitation", "Maximum"),
//            NX_FMT("%1.AreaLimitation.Maximum", path));
//    }
//    catch (const std::exception& exception)
//    {
//        maxSize->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::MissingObjectDetection::Rule::Delay* delay,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        delay->emplaceValue(get<int>(path, rule, "Duration"));
//    }
//    catch (const std::exception& exception)
//    {
//        delay->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(
//    CameraSettings::Vca::MissingObjectDetection::Rule::RequiresHumanInvolvement* requiresHumanInvolvement,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        requiresHumanInvolvement->emplaceValue(get<bool>(path, rule, "HumanFactor"));
//    }
//    catch (const std::exception& exception)
//    {
//        requiresHumanInvolvement->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::MissingObjectDetection* missingObjectDetection,
//    const QJsonValue& parameters)
//{
//    try
//    {
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(parameters, "MissingObjectDetection").isUndefined())
//            get(&jsonRules, parameters, "MissingObjectDetection");
//
//        const auto ruleNames = jsonRules.keys();
//
//        auto& rules = missingObjectDetection->rules;
//        for (std::size_t i = 0; i < rules.size(); ++i)
//        {
//            auto& rule = rules[i];
//            if (i >= (std::size_t) ruleNames.size())
//            {
//                rule.region.emplaceNothing();
//                rule.minSize.emplaceNothing();
//                rule.maxSize.emplaceNothing();
//                rule.delay.emplaceNothing();
//                rule.requiresHumanInvolvement.emplaceNothing();
//                continue;
//            }
//
//            const auto& ruleName = ruleNames[i];
//            const auto& jsonRule = jsonRules[ruleName];
//
//            const QString path = NX_FMT("$.MissingObjectDetection.%1", ruleName);
//
//            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
//            parseReFromCamera(&rule.minSize, jsonRule, path);
//            parseReFromCamera(&rule.maxSize, jsonRule, path);
//            parseReFromCamera(&rule.delay, jsonRule, path);
//            parseReFromCamera(&rule.requiresHumanInvolvement, jsonRule, path);
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(missingObjectDetection, exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::UnattendedObjectDetection::Rule::MinSize* minSize,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        parseReFromCamera(&minSize->emplaceValue(),
//            get<QJsonObject>(path, rule, "AreaLimitation", "Minimum"),
//            NX_FMT("%1.AreaLimitation.Minimum", path));
//    }
//    catch (const std::exception& exception)
//    {
//        minSize->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::UnattendedObjectDetection::Rule::MaxSize* maxSize,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        parseReFromCamera(&maxSize->emplaceValue(),
//            get<QJsonObject>(path, rule, "AreaLimitation", "Maximum"),
//            NX_FMT("%1.AreaLimitation.Maximum", path));
//    }
//    catch (const std::exception& exception)
//    {
//        maxSize->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::UnattendedObjectDetection::Rule::Delay* delay,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        delay->emplaceValue(get<int>(path, rule, "Duration"));
//    }
//    catch (const std::exception& exception)
//    {
//        delay->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(
//    CameraSettings::Vca::UnattendedObjectDetection::Rule::RequiresHumanInvolvement* requiresHumanInvolvement,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        requiresHumanInvolvement->emplaceValue(get<bool>(path, rule, "HumanFactor"));
//    }
//    catch (const std::exception& exception)
//    {
//        requiresHumanInvolvement->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::UnattendedObjectDetection* unattendedObjectDetection,
//    const QJsonValue& parameters)
//{
//    try
//    {
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(parameters, "UnattendedObjectDetection").isUndefined())
//            get(&jsonRules, parameters, "UnattendedObjectDetection");
//
//        const auto ruleNames = jsonRules.keys();
//
//        auto& rules = unattendedObjectDetection->rules;
//        for (std::size_t i = 0; i < rules.size(); ++i)
//        {
//            auto& rule = rules[i];
//            if (i >= (std::size_t) ruleNames.size())
//            {
//                rule.region.emplaceNothing();
//                rule.minSize.emplaceNothing();
//                rule.maxSize.emplaceNothing();
//                rule.delay.emplaceNothing();
//                rule.requiresHumanInvolvement.emplaceNothing();
//                continue;
//            }
//
//            const auto& ruleName = ruleNames[i];
//            const auto& jsonRule = jsonRules[ruleName];
//
//            const QString path = NX_FMT("$.UnattendedObjectDetection.%1", ruleName);
//
//            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
//            parseReFromCamera(&rule.minSize, jsonRule, path);
//            parseReFromCamera(&rule.maxSize, jsonRule, path);
//            parseReFromCamera(&rule.delay, jsonRule, path);
//            parseReFromCamera(&rule.requiresHumanInvolvement, jsonRule, path);
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(unattendedObjectDetection, exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::FaceDetection* faceDetection,
//    const QJsonValue& parameters)
//{
//    try
//    {
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(parameters, "FaceDetection").isUndefined())
//            get(&jsonRules, parameters, "FaceDetection");
//
//        const auto ruleNames = jsonRules.keys();
//
//        auto& rules = faceDetection->rules;
//        for (std::size_t i = 0; i < rules.size(); ++i)
//        {
//            auto& rule = rules[i];
//            if (i >= (std::size_t) ruleNames.size())
//            {
//                rule.region.emplaceNothing();
//                continue;
//            }
//
//            const auto& ruleName = ruleNames[i];
//            const auto& jsonRule = jsonRules[ruleName];
//
//            const QString path = NX_FMT("$.FaceDetection.%1", ruleName);
//
//            parseReFromCamera(&rule.region, jsonRule, ruleName, path);
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(faceDetection, exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::RunningDetection::Rule::MinCount* minCount,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        minCount->emplaceValue(get<int>(path, rule, "PeopleNumber"));
//    }
//    catch (const std::exception& exception)
//    {
//        minCount->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::RunningDetection::Rule::MinSpeed* minSpeed,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        minSpeed->emplaceValue(get<int>(path, rule, "SpeedLevel"));
//    }
//    catch (const std::exception& exception)
//    {
//        minSpeed->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::RunningDetection::Rule::Delay* delay,
//    const QJsonValue& rule, const QString& path)
//{
//    try
//    {
//        delay->emplaceValue(get<int>(path, rule, "MinActivityDuration"));
//    }
//    catch (const std::exception& exception)
//    {
//        delay->emplaceErrorMessage(exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca::RunningDetection* runningDetection,
//    const QJsonValue& parameters)
//{
//    try
//    {
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(parameters, "RunningDetection").isUndefined())
//            get(&jsonRules, parameters, "RunningDetection");
//
//        const auto ruleNames = jsonRules.keys();
//
//        auto& rules = runningDetection->rules;
//        for (std::size_t i = 0; i < rules.size(); ++i)
//        {
//            auto& rule = rules[i];
//            if (i >= (std::size_t) ruleNames.size())
//            {
//                rule.name.emplaceNothing();
//                rule.minCount.emplaceNothing();
//                rule.minSpeed.emplaceNothing();
//                rule.delay.emplaceNothing();
//                continue;
//            }
//
//            const auto& ruleName = ruleNames[i];
//            const auto& jsonRule = jsonRules[ruleName];
//
//            const QString path = NX_FMT("$.RunningDetection.%1", ruleName);
//
//            rule.name.emplaceValue(ruleName);
//            parseReFromCamera(&rule.minCount, jsonRule, path);
//            parseReFromCamera(&rule.minSpeed, jsonRule, path);
//            parseReFromCamera(&rule.delay, jsonRule, path);
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(runningDetection, exception.what());
//    }
//}
//
//void parseReFromCamera(CameraSettings::Vca* vca, const QJsonValue& parameters)
//{
//    parseReFromCamera(&vca->installation, parameters);
//    if (auto& crowdDetection = vca->crowdDetection)
//        parseReFromCamera(&*crowdDetection, parameters);
//    if (auto& loiteringDetection = vca->loiteringDetection)
//        parseReFromCamera(&*loiteringDetection, parameters);
//    if (auto& intrusionDetection = vca->intrusionDetection)
//        parseReFromCamera(&*intrusionDetection, parameters);
//    if (auto& lineCrossingDetection = vca->lineCrossingDetection)
//        parseReFromCamera(&*lineCrossingDetection, parameters);
//    if (auto& missingObjectDetection = vca->missingObjectDetection)
//        parseReFromCamera(&*missingObjectDetection, parameters);
//    if (auto& unattendedObjectDetection = vca->unattendedObjectDetection)
//        parseReFromCamera(&*unattendedObjectDetection, parameters);
//    if (auto& faceDetection = vca->faceDetection)
//        parseReFromCamera(&*faceDetection, parameters);
//    if (auto& runningDetection = vca->runningDetection)
//        parseReFromCamera(&*runningDetection, parameters);
//}
//
//
//void fetchReFromCamera(CameraSettings::Vca* vca, CameraVcaParameterApi* api)
//{
//    try
//    {
//        const auto parameters = api->fetch("Config/RE").get();
//
//        parseReFromCamera(vca, parameters);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(vca, exception.what());
//    }
//}
//
//void fetchFromCamera(CameraSettings::Vca* vca, const Url& cameraUrl)
//{
//    auto& enabled = vca->enabled;
//    fetchFromCamera(&enabled, cameraUrl);
//    if (!enabled.hasValue() || !enabled.value())
//        return;
//
//    CameraVcaParameterApi api(cameraUrl);
//
//    fetchAeFromCamera(vca, &api);
//    fetchReFromCamera(vca, &api);
//}
//
//void storeToCamera(const Url& cameraUrl, CameraSettings::Vca::Enabled* enabled)
//{
//    try
//    {
//        if (!enabled->hasValue())
//            return;
//
//        const auto moduleInfo = VadpModuleInfo::fetchFrom(cameraUrl, "VCA");
//        if (moduleInfo.enabled == enabled->value())
//            return;
//
//        QUrlQuery query;
//        query.addQueryItem("idx", QString::number(moduleInfo.index));
//        query.addQueryItem("cmd", enabled->value() ? "start" : "stop");
//
//        auto url = cameraUrl;
//        url.setPath("/cgi-bin/admin/vadpctrl.cgi");
//        url.setQuery(query);
//
//        QByteArray responseBody;
//        try
//        {
//            responseBody = HttpClient().get(url).get();
//        }
//        catch (const std::system_error& exception)
//        {
//            // Firmware versions before 0121 return malformed HTTP response. From here it looks as
//            // if it resets the connection before the response could be parsed.
//            if (exception.code() != std::errc::connection_reset)
//                throw;
//
//            static const QString firstFixedVersion = "0121";
//            const auto version = getFirmwareVersion(cameraUrl);
//            if (version >= firstFixedVersion)
//                throw;
//
//            enabled->emplaceErrorMessage(NX_FMT(
//                "The camera is running firmware version %1, which contains some bugs, please "
//                "update it to at least version %2", version, firstFixedVersion));
//            return;
//        }
//
//        const auto successPattern =
//            NX_FMT("'VCA is %1'", enabled->value() ? "started" : "stopped").toUtf8();
//        if (!responseBody.contains(successPattern))
//        {
//            throw Exception(
//                "HTTP response body doesn't contain expected pattern indicating success");
//        }
//    }
//    catch (const std::exception& exception)
//    {
//        enabled->emplaceErrorMessage(exception.what());
//    }
//}
//
//
//auto serializeAeToCamera(const CameraSettings::Vca::Installation::Height& entry)
//{
//    return entry.value() * 10; // API uses millimeters, while web UI uses centimeters.
//}
//
//template <typename Value>
//auto serializeAeToCamera(const CameraSettings::Entry<Value>& entry)
//{
//    return entry.value();
//}
//
//
//void storeAeToCamera(CameraVcaParameterApi* api, CameraSettings::Vca* vca)
//{
//    try
//    {
//        auto parameters = api->fetch("Config/AE").get();
//
//        enumerateAeEntries(vca,
//            [&](auto* entry, const auto&... keys)
//            {
//                try
//                {
//                    if (!entry->hasValue())
//                        return;
//
//                    set(&parameters, keys..., serializeAeToCamera(*entry));
//                }
//                catch (const std::exception& exception)
//                {
//                    entry->emplaceErrorMessage(exception.what());
//                }
//            });
//
//        api->store("Config/AE", parameters).get();
//    }
//    catch (const std::exception& exception)
//    {
//        const QString message = exception.what();
//        enumerateAeEntries(vca,
//            [&](auto* entry, auto&&...) { entry->emplaceErrorMessage(message); });
//    }
//}
//
//
//template <typename NamedThing>
//std::optional<QString> getName(const CameraSettings::Entry<NamedThing>& entry)
//{
//    if (!entry.hasValue())
//        return std::nullopt;
//
//    const auto& thing = entry.value();
//    return thing.name;
//}
//
//template <typename Rule>
//auto getName(const Rule& rule) -> decltype(rule.name.value(), void(), std::optional<QString>())
//{
//    if (!rule.name.hasValue())
//        return std::nullopt;
//
//    return rule.name.value();
//}
//
//template <typename Rule>
//auto getName(const Rule& rule) -> decltype(getName(rule.region))
//{
//    return getName(rule.region);
//}
//
//template <typename Rule>
//auto getName(const Rule& rule) -> decltype(getName(rule.line))
//{
//    return getName(rule.line);
//}
//
//template <typename Rule>
//std::set<QString> getNames(const std::vector<Rule>& rules)
//{
//    std::set<QString> names;
//    for (const auto& rule: rules)
//    {
//        if (auto name = getName(rule))
//            names.insert(*name);
//    }
//    return names;
//}
//
//void removeRulesExcept(QJsonObject* rules, const std::set<QString>& allowedNames)
//{
//    for (const auto& name: rules->keys())
//    {
//        if (!allowedNames.count(name))
//            rules->remove(name);
//    }
//}
//
//
//void serializeReToCamera(QJsonValue* parameters,
//    CameraSettings::Vca::Installation* installation)
//{
//    try
//    {
//        QJsonArray field;
//        for (const auto& exclusion: installation->exclusions)
//        {
//            const auto& region = exclusion.region;
//            if (!region.hasValue())
//                continue;
//
//            QJsonArray subField;
//            for (const auto& point: region.value())
//                subField.push_back(CameraVcaParameterApi::serialize(point));
//
//            field.push_back(std::move(subField));
//        }
//        set(parameters, "ExclusiveArea", QJsonObject{
//            {"Field", field},
//        });
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(installation, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Entry<NamedPolygon>* region)
//{
//    try
//    {
//        if (!region->hasValue())
//            return;
//
//        set(jsonRule, "RuleName", region->value().name);
//        set(jsonRule, "EventName", region->value().name);
//
//        set(jsonRule, "Field3D", QJsonValue::Undefined);
//
//        QJsonArray field;
//        for (const auto& point: region->value())
//            field.push_back(CameraVcaParameterApi::serialize(point));
//
//        // Wrapping in another array is intentional. For some reason, camera expects each region
//        // as an array of a single array of points.
//        set(jsonRule, "Field", QJsonArray{field});
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(region, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::CrowdDetection::Rule::SizeThreshold* sizeThreshold)
//{
//    try
//    {
//        // If new rule, set default like in web UI.
//        if (sizeThreshold->hasNothing()
//            && get<QJsonValue>(*jsonRule, "PeopleNumber").isUndefined())
//        {
//            sizeThreshold->emplaceValue(2);
//        }
//
//        if (!sizeThreshold->hasValue())
//            return;
//
//        set(jsonRule, "PeopleNumber", sizeThreshold->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(sizeThreshold, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::CrowdDetection::Rule::EnterDelay* enterDelay)
//{
//    try
//    {
//        if (!enterDelay->hasValue())
//            return;
//
//        set(jsonRule, "EnterDelay", enterDelay->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(enterDelay, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::CrowdDetection::Rule::ExitDelay* exitDelay)
//{
//    try
//    {
//        // If new rule, set default like in web UI.
//        if (exitDelay->hasNothing()
//            && get<QJsonValue>(*jsonRule, "LeaveDelay").isUndefined())
//        {
//            exitDelay->emplaceValue(1);
//        }
//
//        if (!exitDelay->hasValue())
//            return;
//
//        set(jsonRule, "LeaveDelay", exitDelay->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(exitDelay, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* parameters,
//    CameraSettings::Vca::CrowdDetection* crowdDetection)
//{
//    try
//    {
//        auto& rules = crowdDetection->rules;
//
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(*parameters, "CrowdDetection").isUndefined())
//        {
//            jsonRules = get<QJsonObject>(*parameters, "CrowdDetection");
//            removeRulesExcept(&jsonRules, getNames(rules));
//        }
//
//        for (auto& rule: rules)
//        {
//            if (auto name = getName(rule))
//            {
//                QJsonValue jsonRule = jsonRules[*name];
//                if (jsonRule.isUndefined() || jsonRule.isNull())
//                    jsonRule = QJsonObject{};
//
//                serializeReToCamera(&jsonRule, &rule.region);
//                serializeReToCamera(&jsonRule, &rule.sizeThreshold);
//                serializeReToCamera(&jsonRule, &rule.enterDelay);
//                serializeReToCamera(&jsonRule, &rule.exitDelay);
//
//                jsonRules[*name] = jsonRule;
//            }
//        }
//
//        set(parameters, "CrowdDetection", jsonRules);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(crowdDetection, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::LoiteringDetection::Rule::Delay* delay)
//{
//    try
//    {
//        // If new rule, set default like in web UI.
//        if (delay->hasNothing()
//            && get<QJsonValue>(*jsonRule, "StayTime").isUndefined())
//        {
//            delay->emplaceValue(5);
//        }
//
//        if (!delay->hasValue())
//            return;
//
//        set(jsonRule, "StayTime", delay->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(delay, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* parameters,
//    CameraSettings::Vca::LoiteringDetection* loiteringDetection)
//{
//    try
//    {
//        auto& rules = loiteringDetection->rules;
//
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(*parameters, "LoiteringDetection").isUndefined())
//        {
//            jsonRules = get<QJsonObject>(*parameters, "LoiteringDetection");
//            removeRulesExcept(&jsonRules, getNames(rules));
//        }
//
//        for (auto& rule: rules)
//        {
//            if (auto name = getName(rule))
//            {
//                QJsonValue jsonRule = jsonRules[*name];
//                if (jsonRule.isUndefined() || jsonRule.isNull())
//                    jsonRule = QJsonObject{};
//
//                serializeReToCamera(&jsonRule, &rule.region);
//                serializeReToCamera(&jsonRule, &rule.delay);
//
//                jsonRules[*name] = jsonRule;
//            }
//        }
//
//        set(parameters, "LoiteringDetection", jsonRules);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(loiteringDetection, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::IntrusionDetection::Rule::Inverted* inverted)
//{
//    try
//    {
//        // If new rule, set default like in web UI.
//        if (inverted->hasNothing()
//            && get<QJsonValue>(*jsonRule, "WalkingDirection").isUndefined())
//        {
//            inverted->emplaceValue(false);
//        }
//
//        if (!inverted->hasValue())
//            return;
//
//        set(jsonRule, "WalkingDirection", inverted->value() ? "InToOut" : "OutToIn");
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(inverted, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* parameters,
//    CameraSettings::Vca::IntrusionDetection* intrusionDetection)
//{
//    try
//    {
//        auto& rules = intrusionDetection->rules;
//
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(*parameters, "IntrusionDetection").isUndefined())
//        {
//            jsonRules = get<QJsonObject>(*parameters, "IntrusionDetection");
//            removeRulesExcept(&jsonRules, getNames(rules));
//        }
//
//        for (auto& rule: rules)
//        {
//            if (auto name = getName(rule))
//            {
//                QJsonValue jsonRule = jsonRules[*name];
//                if (jsonRule.isUndefined() || jsonRule.isNull())
//                    jsonRule = QJsonObject{};
//
//                serializeReToCamera(&jsonRule, &rule.region);
//                serializeReToCamera(&jsonRule, &rule.inverted);
//
//                jsonRules[*name] = jsonRule;
//            }
//        }
//
//        set(parameters, "IntrusionDetection", jsonRules);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(intrusionDetection, exception.what());
//    }
//}
//
//QString serializeReToCamera(NamedLine::Direction direction)
//{
//    switch (direction)
//    {
//        case NamedLine::Direction::any:
//            return "Any";
//        case NamedLine::Direction::leftToRight:
//            return "Out";
//        case NamedLine::Direction::rightToLeft:
//            return "In";
//        default:
//            NX_ASSERT(false, "Unknown NamedLine::Direction value: %1", (int) direction);
//            return "";
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Entry<NamedLine>* line)
//{
//    try
//    {
//        if (!line->hasValue())
//            return;
//
//        set(jsonRule, "RuleName", line->value().name);
//        set(jsonRule, "EventName", line->value().name);
//
//        set(jsonRule, "Line3D", QJsonValue::Undefined);
//
//        QJsonArray jsonLine;
//        for (const auto& point: line->value())
//            jsonLine.push_back(CameraVcaParameterApi::serialize(point));
//
//        // Wrapping in another array is intentional. For some reason, camera expects each line
//        // as an array of a single array of points.
//        set(jsonRule, "Line", QJsonArray{jsonLine});
//
//        set(jsonRule, "Direction", serializeReToCamera(line->value().direction));
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(line, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* parameters,
//    CameraSettings::Vca::LineCrossingDetection* lineCrossingDetection)
//{
//    try
//    {
//        auto& rules = lineCrossingDetection->rules;
//
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(*parameters, "LineCrossingDetection").isUndefined())
//        {
//            jsonRules = get<QJsonObject>(*parameters, "LineCrossingDetection");
//            removeRulesExcept(&jsonRules, getNames(rules));
//        }
//
//        for (auto& rule: rules)
//        {
//            if (auto name = getName(rule))
//            {
//                QJsonValue jsonRule = jsonRules[*name];
//                if (jsonRule.isUndefined() || jsonRule.isNull())
//                    jsonRule = QJsonObject{};
//
//                serializeReToCamera(&jsonRule, &rule.line);
//
//                jsonRules[*name] = jsonRule;
//            }
//        }
//
//        set(parameters, "LineCrossingDetection", jsonRules);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(lineCrossingDetection, exception.what());
//    }
//}
//
//QJsonObject serializeReToCamera(const Rect& rect)
//{
//    return QJsonObject{
//        {"x", CameraVcaParameterApi::serializeCoordinate(rect.x)},
//        {"y", CameraVcaParameterApi::serializeCoordinate(rect.y)},
//        {"w", CameraVcaParameterApi::serializeCoordinate(rect.width)},
//        {"h", CameraVcaParameterApi::serializeCoordinate(rect.height)},
//    };
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::MissingObjectDetection::Rule::MinSize* minSize)
//{
//    try
//    {
//        if (!minSize->hasValue())
//            return;
//
//        QJsonObject areaLimitation;
//        if (!get<QJsonValue>(*jsonRule, "AreaLimitation").isUndefined())
//            get(&areaLimitation, *jsonRule, "AreaLimitation");
//
//        areaLimitation["Minimum"] = serializeReToCamera(minSize->value());
//
//        set(jsonRule, "AreaLimitation", areaLimitation);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(minSize, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::MissingObjectDetection::Rule::MaxSize* maxSize)
//{
//    try
//    {
//        if (!maxSize->hasValue())
//            return;
//
//        QJsonObject areaLimitation;
//        if (!get<QJsonValue>(*jsonRule, "AreaLimitation").isUndefined())
//            get(&areaLimitation, *jsonRule, "AreaLimitation");
//
//        areaLimitation["Maximum"] = serializeReToCamera(maxSize->value());
//
//        set(jsonRule, "AreaLimitation", areaLimitation);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(maxSize, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::MissingObjectDetection::Rule::Delay* delay)
//{
//    try
//    {
//        // If new rule, set default like in web UI.
//        if (delay->hasNothing()
//            && get<QJsonValue>(*jsonRule, "Duration").isUndefined())
//        {
//            delay->emplaceValue(300);
//        }
//
//        if (!delay->hasValue())
//            return;
//
//        set(jsonRule, "Duration", delay->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(delay, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::MissingObjectDetection::Rule::RequiresHumanInvolvement* requiresHumanInvolvement)
//{
//    try
//    {
//        if (!requiresHumanInvolvement->hasValue())
//            return;
//
//        set(jsonRule, "HumanFactor", requiresHumanInvolvement->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(requiresHumanInvolvement, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* parameters,
//    CameraSettings::Vca::MissingObjectDetection* missingObjectDetection)
//{
//    try
//    {
//        auto& rules = missingObjectDetection->rules;
//
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(*parameters, "MissingObjectDetection").isUndefined())
//        {
//            jsonRules = get<QJsonObject>(*parameters, "MissingObjectDetection");
//            removeRulesExcept(&jsonRules, getNames(rules));
//        }
//
//        for (auto& rule: rules)
//        {
//            if (auto name = getName(rule))
//            {
//                QJsonValue jsonRule = jsonRules[*name];
//                if (jsonRule.isUndefined() || jsonRule.isNull())
//                    jsonRule = QJsonObject{};
//
//                serializeReToCamera(&jsonRule, &rule.region);
//                serializeReToCamera(&jsonRule, &rule.minSize);
//                serializeReToCamera(&jsonRule, &rule.maxSize);
//                serializeReToCamera(&jsonRule, &rule.delay);
//                serializeReToCamera(&jsonRule, &rule.requiresHumanInvolvement);
//
//                jsonRules[*name] = jsonRule;
//            }
//        }
//
//        set(parameters, "MissingObjectDetection", jsonRules);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(missingObjectDetection, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::UnattendedObjectDetection::Rule::MinSize* minSize)
//{
//    try
//    {
//        if (!minSize->hasValue())
//            return;
//
//        QJsonObject areaLimitation;
//        if (!get<QJsonValue>(*jsonRule, "AreaLimitation").isUndefined())
//            get(&areaLimitation, *jsonRule, "AreaLimitation");
//
//        areaLimitation["Minimum"] = serializeReToCamera(minSize->value());
//
//        set(jsonRule, "AreaLimitation", areaLimitation);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(minSize, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::UnattendedObjectDetection::Rule::MaxSize* maxSize)
//{
//    try
//    {
//        if (!maxSize->hasValue())
//            return;
//
//        QJsonObject areaLimitation;
//        if (!get<QJsonValue>(*jsonRule, "AreaLimitation").isUndefined())
//            get(&areaLimitation, *jsonRule, "AreaLimitation");
//
//        areaLimitation["Maximum"] = serializeReToCamera(maxSize->value());
//
//        set(jsonRule, "AreaLimitation", areaLimitation);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(maxSize, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::UnattendedObjectDetection::Rule::Delay* delay)
//{
//    try
//    {
//        // If new rule, set default like in web UI.
//        if (delay->hasNothing()
//            && get<QJsonValue>(*jsonRule, "Duration").isUndefined())
//        {
//            delay->emplaceValue(300);
//        }
//
//        if (!delay->hasValue())
//            return;
//
//        set(jsonRule, "Duration", delay->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(delay, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::UnattendedObjectDetection::Rule::RequiresHumanInvolvement* requiresHumanInvolvement)
//{
//    try
//    {
//        if (!requiresHumanInvolvement->hasValue())
//            return;
//
//        set(jsonRule, "HumanFactor", requiresHumanInvolvement->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(requiresHumanInvolvement, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* parameters,
//    CameraSettings::Vca::UnattendedObjectDetection* unattendedObjectDetection)
//{
//    try
//    {
//        auto& rules = unattendedObjectDetection->rules;
//
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(*parameters, "UnattendedObjectDetection").isUndefined())
//        {
//            jsonRules = get<QJsonObject>(*parameters, "UnattendedObjectDetection");
//            removeRulesExcept(&jsonRules, getNames(rules));
//        }
//
//        for (auto& rule: rules)
//        {
//            if (auto name = getName(rule))
//            {
//                QJsonValue jsonRule = jsonRules[*name];
//                if (jsonRule.isUndefined() || jsonRule.isNull())
//                    jsonRule = QJsonObject{};
//
//                serializeReToCamera(&jsonRule, &rule.region);
//                serializeReToCamera(&jsonRule, &rule.minSize);
//                serializeReToCamera(&jsonRule, &rule.maxSize);
//                serializeReToCamera(&jsonRule, &rule.delay);
//                serializeReToCamera(&jsonRule, &rule.requiresHumanInvolvement);
//
//                jsonRules[*name] = jsonRule;
//            }
//        }
//
//        set(parameters, "UnattendedObjectDetection", jsonRules);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(unattendedObjectDetection, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* parameters,
//    CameraSettings::Vca::FaceDetection* faceDetection)
//{
//    try
//    {
//        auto& rules = faceDetection->rules;
//
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(*parameters, "FaceDetection").isUndefined())
//        {
//            jsonRules = get<QJsonObject>(*parameters, "FaceDetection");
//            removeRulesExcept(&jsonRules, getNames(rules));
//        }
//
//        for (auto& rule: rules)
//        {
//            if (auto name = getName(rule))
//            {
//                QJsonValue jsonRule = jsonRules[*name];
//                if (jsonRule.isUndefined() || jsonRule.isNull())
//                    jsonRule = QJsonObject{};
//
//                serializeReToCamera(&jsonRule, &rule.region);
//
//                jsonRules[*name] = jsonRule;
//            }
//        }
//
//        set(parameters, "FaceDetection", jsonRules);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(faceDetection, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::RunningDetection::Rule::MinCount* minCount)
//{
//    try
//    {
//        // If new rule, set default like in web UI.
//        if (minCount->hasNothing()
//            && get<QJsonValue>(*jsonRule, "PeopleNumber").isUndefined())
//        {
//            minCount->emplaceValue(1);
//        }
//
//        if (!minCount->hasValue())
//            return;
//
//        set(jsonRule, "PeopleNumber", minCount->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(minCount, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::RunningDetection::Rule::MinSpeed* minSpeed)
//{
//    try
//    {
//        // If new rule, set default like in web UI.
//        if (minSpeed->hasNothing()
//            && get<QJsonValue>(*jsonRule, "SpeedLevel").isUndefined())
//        {
//            minSpeed->emplaceValue(3);
//        }
//
//        if (!minSpeed->hasValue())
//            return;
//
//        set(jsonRule, "SpeedLevel", minSpeed->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(minSpeed, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* jsonRule,
//    CameraSettings::Vca::RunningDetection::Rule::Delay* delay)
//{
//    try
//    {
//        // If new rule, set default like in web UI.
//        if (delay->hasNothing()
//            && get<QJsonValue>(*jsonRule, "MinActivityDuration").isUndefined())
//        {
//            delay->emplaceValue(1);
//        }
//
//        if (!delay->hasValue())
//            return;
//
//        set(jsonRule, "MinActivityDuration", delay->value());
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(delay, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* parameters,
//    CameraSettings::Vca::RunningDetection* runningDetection)
//{
//    try
//    {
//        auto& rules = runningDetection->rules;
//
//        QJsonObject jsonRules;
//        if (!get<QJsonValue>(*parameters, "RunningDetection").isUndefined())
//        {
//            jsonRules = get<QJsonObject>(*parameters, "RunningDetection");
//            removeRulesExcept(&jsonRules, getNames(rules));
//        }
//
//        for (auto& rule: rules)
//        {
//            if (auto name = getName(rule))
//            {
//                QJsonValue jsonRule = jsonRules[*name];
//                if (jsonRule.isUndefined() || jsonRule.isNull())
//                    jsonRule = QJsonObject{};
//
//                serializeReToCamera(&jsonRule, &rule.minCount);
//                serializeReToCamera(&jsonRule, &rule.minSpeed);
//                serializeReToCamera(&jsonRule, &rule.delay);
//
//                jsonRules[*name] = jsonRule;
//            }
//        }
//
//        set(parameters, "RunningDetection", jsonRules);
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(runningDetection, exception.what());
//    }
//}
//
//void serializeReToCamera(QJsonValue* parameters, CameraSettings::Vca* vca)
//{
//    serializeReToCamera(parameters, &vca->installation);
//    if (auto& crowdDetection = vca->crowdDetection)
//        serializeReToCamera(parameters, &*crowdDetection);
//    if (auto& loiteringDetection = vca->loiteringDetection)
//        serializeReToCamera(parameters, &*loiteringDetection);
//    if (auto& intrusionDetection = vca->intrusionDetection)
//        serializeReToCamera(parameters, &*intrusionDetection);
//    if (auto& lineCrossingDetection = vca->lineCrossingDetection)
//        serializeReToCamera(parameters, &*lineCrossingDetection);
//    if (auto& missingObjectDetection = vca->missingObjectDetection)
//        serializeReToCamera(parameters, &*missingObjectDetection);
//    if (auto& unattendedObjectDetection = vca->unattendedObjectDetection)
//        serializeReToCamera(parameters, &*unattendedObjectDetection);
//    if (auto& faceDetection = vca->faceDetection)
//        serializeReToCamera(parameters, &*faceDetection);
//    if (auto& runningDetection = vca->runningDetection)
//        serializeReToCamera(parameters, &*runningDetection);
//}
//
//
//void storeReToCamera(CameraVcaParameterApi* api, CameraSettings::Vca* vca)
//{
//    try
//    {
//        auto parameters = api->fetch("Config/RE").get();
//
//        serializeReToCamera(&parameters, vca);
//
//        api->store("Config/RE", parameters).get();
//    }
//    catch (const std::exception& exception)
//    {
//        fillReErrors(vca, exception.what());
//    }
//}
//
//
//void storeToCamera(const Url& cameraUrl, CameraSettings::Vca* vca)
//{
//    auto& enabled = vca->enabled;
//    storeToCamera(cameraUrl, &enabled);
//    if (!enabled.hasValue() || !enabled.value())
//        return;
//
//    CameraVcaParameterApi api(cameraUrl);
//
//    storeAeToCamera(&api, vca);
//    storeReToCamera(&api, vca);
//
//    api.reloadConfig().get();
//}
//
//
//template <typename Settings, typename Visitor>
//void enumerateEntries(Settings* settings, Visitor visit)
//{
//    const auto uniqueVisit =
//        [&](auto* entry)
//        {
//            visit(QString(entry->name), entry);
//        };
//
//    const auto repeatedVisit =
//        [&](std::size_t index, auto* entry)
//        {
//            visit(QString(entry->name).replace("#", "%1").arg(index + 1), entry);
//        };
//
//    if (auto& vca = settings->vca)
//    {
//        uniqueVisit(&vca->enabled);
//        uniqueVisit(&vca->sensitivity);
//
//        auto& installation = vca->installation;
//        uniqueVisit(&installation.height);
//        uniqueVisit(&installation.tiltAngle);
//        uniqueVisit(&installation.rollAngle);
//        auto& exclusions = installation.exclusions;
//        for (std::size_t i = 0; i < exclusions.size(); ++i)
//        {
//            auto& exclusion = exclusions[i];
//
//            repeatedVisit(i, &exclusion.region);
//        }
//
//        if (auto& crowdDetection = vca->crowdDetection)
//        {
//            auto& rules = crowdDetection->rules;
//            for (std::size_t i = 0; i < rules.size(); ++i)
//            {
//                auto& rule = rules[i];
//
//                repeatedVisit(i, &rule.region);
//                repeatedVisit(i, &rule.sizeThreshold);
//                repeatedVisit(i, &rule.enterDelay);
//                repeatedVisit(i, &rule.exitDelay);
//            }
//        }
//
//        if (auto& loiteringDetection = vca->loiteringDetection)
//        {
//            auto& rules = loiteringDetection->rules;
//            for (std::size_t i = 0; i < rules.size(); ++i)
//            {
//                auto& rule = rules[i];
//
//                repeatedVisit(i, &rule.region);
//                repeatedVisit(i, &rule.delay);
//            }
//        }
//
//        if (auto& intrusionDetection = vca->intrusionDetection)
//        {
//            auto& rules = intrusionDetection->rules;
//            for (std::size_t i = 0; i < rules.size(); ++i)
//            {
//                auto& rule = rules[i];
//
//                repeatedVisit(i, &rule.region);
//                repeatedVisit(i, &rule.inverted);
//            }
//        }
//
//        if (auto& lineCrossingDetection = vca->lineCrossingDetection)
//        {
//            auto& rules = lineCrossingDetection->rules;
//            for (std::size_t i = 0; i < rules.size(); ++i)
//            {
//                auto& rule = rules[i];
//
//                repeatedVisit(i, &rule.line);
//            }
//        }
//
//        if (auto& missingObjectDetection = vca->missingObjectDetection)
//        {
//            auto& rules = missingObjectDetection->rules;
//            for (std::size_t i = 0; i < rules.size(); ++i)
//            {
//                auto& rule = rules[i];
//
//                repeatedVisit(i, &rule.region);
//                repeatedVisit(i, &rule.minSize);
//                repeatedVisit(i, &rule.maxSize);
//                repeatedVisit(i, &rule.delay);
//                repeatedVisit(i, &rule.requiresHumanInvolvement);
//            }
//        }
//
//        if (auto& unattendedObjectDetection = vca->unattendedObjectDetection)
//        {
//            auto& rules = unattendedObjectDetection->rules;
//            for (std::size_t i = 0; i < rules.size(); ++i)
//            {
//                auto& rule = rules[i];
//
//                repeatedVisit(i, &rule.region);
//                repeatedVisit(i, &rule.minSize);
//                repeatedVisit(i, &rule.maxSize);
//                repeatedVisit(i, &rule.delay);
//                repeatedVisit(i, &rule.requiresHumanInvolvement);
//            }
//        }
//
//        if (auto& faceDetection = vca->faceDetection)
//        {
//            auto& rules = faceDetection->rules;
//            for (std::size_t i = 0; i < rules.size(); ++i)
//            {
//                auto& rule = rules[i];
//
//                repeatedVisit(i, &rule.region);
//            }
//        }
//
//        if (auto& runningDetection = vca->runningDetection)
//        {
//            auto& rules = runningDetection->rules;
//            for (std::size_t i = 0; i < rules.size(); ++i)
//            {
//                auto& rule = rules[i];
//
//                repeatedVisit(i, &rule.name);
//                repeatedVisit(i, &rule.minCount);
//                repeatedVisit(i, &rule.minSpeed);
//                repeatedVisit(i, &rule.delay);
//            }
//        }
//    }
//}
//
//
//void parseEntryFromServer(
//    CameraSettings::Vca::IntrusionDetection::Rule::Inverted* entry, const QString& serializedValue)
//{
//    if (serializedValue == "")
//        entry->emplaceNothing();
//    else if (serializedValue == "false")
//        entry->emplaceValue(false);
//    else if (serializedValue == "true")
//        entry->emplaceValue(true);
//    else
//        throw Exception("Failed to parse boolean");
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::MissingObjectDetection::Rule::RequiresHumanInvolvement* entry,
//    const QString& serializedValue)
//{
//    if (serializedValue == "")
//        entry->emplaceNothing();
//    else if (serializedValue == "false")
//        entry->emplaceValue(false);
//    else if (serializedValue == "true")
//        entry->emplaceValue(true);
//    else
//        throw Exception("Failed to parse boolean");
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::UnattendedObjectDetection::Rule::RequiresHumanInvolvement* entry,
//    const QString& serializedValue)
//{
//    if (serializedValue == "")
//        entry->emplaceNothing();
//    else if (serializedValue == "false")
//        entry->emplaceValue(false);
//    else if (serializedValue == "true")
//        entry->emplaceValue(true);
//    else
//        throw Exception("Failed to parse boolean");
//}
//
//void parseEntryFromServer(
//    CameraSettings::Entry<bool>* entry, const QString& serializedValue)
//{
//    if (serializedValue == "false")
//        entry->emplaceValue(false);
//    else if (serializedValue == "true")
//        entry->emplaceValue(true);
//    else
//        throw Exception("Failed to parse boolean");
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::Sensitivity* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 1, 10));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::Installation::Height* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 2000));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::Installation::TiltAngle* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 179));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::Installation::RollAngle* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), -74, +74));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::CrowdDetection::Rule::SizeThreshold* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 20));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::CrowdDetection::Rule::EnterDelay* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::CrowdDetection::Rule::ExitDelay* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::LoiteringDetection::Rule::Delay* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::MissingObjectDetection::Rule::Delay* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::UnattendedObjectDetection::Rule::Delay* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::RunningDetection::Rule::MinCount* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 1, 3));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::RunningDetection::Rule::MinSpeed* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 6));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::RunningDetection::Rule::Delay* entry, const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::clamp((int) toDouble(serializedValue), 0, 999));
//}
//
//void parseEntryFromServer(CameraSettings::Entry<int>* entry, const QString& serializedValue)
//{
//    entry->emplaceValue(toInt(serializedValue));
//}
//
//void parseEntryFromServer(CameraSettings::Vca::RunningDetection::Rule::Name* entry,
//    const QString& serializedValue)
//{
//    if (serializedValue.isEmpty())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(serializedValue);
//}
//
//bool parseFromServer(std::vector<Point>* points, const QJsonValue& json)
//{
//    const auto figure = get<QJsonValue>(json, "figure");
//    if (figure.isNull() || figure.isUndefined())
//        return false;
//
//    const auto jsonPoints = get<QJsonArray>("$.figure", figure, "points");
//    for (int i = 0; i < jsonPoints.count(); ++i)
//    {
//        const auto point = get<QJsonArray>("$.figure.points", jsonPoints, i);
//        const QString path = NX_FMT("$.figure.points[%1]", i);
//        points->emplace_back(
//            get<double>(path, point, 0),
//            get<double>(path, point, 1));
//    }
//    
//    return true;
//}
//
//void parseEntryFromServer(
//    CameraSettings::Vca::Installation::Exclusion::Region* entry, const QString& serializedValue)
//{
//    const auto json = parseJson(serializedValue.toUtf8());
//
//    NamedPolygon polygon;
//    if (!parseFromServer(&polygon, json))
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    entry->emplaceValue(std::move(polygon));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Entry<NamedPolygon>* entry, const QString& serializedValue)
//{
//    const auto json = parseJson(serializedValue.toUtf8());
//
//    NamedPolygon polygon;
//    if (!parseFromServer(&polygon, json))
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    polygon.name = get<QString>(json, "label");
//    if (polygon.name.isEmpty())
//        throw Exception("Empty name");
//
//    entry->emplaceValue(std::move(polygon));
//}
//
//void parseFromServer(NamedLine::Direction* direction, const QString& serializedValue)
//{
//    if (serializedValue == "absent")
//        *direction = NamedLine::Direction::any;
//    else if (serializedValue == "right")
//        *direction = NamedLine::Direction::leftToRight;
//    else if (serializedValue == "left")
//        *direction = NamedLine::Direction::rightToLeft;
//    else
//        throw Exception("Unknown LineFigure direction: %1", serializedValue);
//}
//
//void parseEntryFromServer(
//    CameraSettings::Entry<NamedLine>* entry, const QString& serializedValue)
//{
//    const auto json = parseJson(serializedValue.toUtf8());
//
//    NamedLine line;
//    if (!parseFromServer(&line, json))
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    line.name = get<QString>(json, "label");
//    if (line.name.isEmpty())
//        throw Exception("Empty name");
//
//    parseFromServer(&line.direction, get<QString>(json, "figure", "direction"));
//
//    entry->emplaceValue(std::move(line));
//}
//
//void parseEntryFromServer(
//    CameraSettings::Entry<Rect>* entry, const QString& serializedValue)
//{
//    const auto json = parseJson(serializedValue.toUtf8());
//
//    const auto figure = get<QJsonValue>(json, "figure");
//    if (figure.isNull() || figure.isUndefined())
//    {
//        entry->emplaceNothing();
//        return;
//    }
//
//    const auto jsonPoints = get<QJsonArray>("$.figure", figure, "points");
//    if (jsonPoints.count() != 2)
//        throw Exception("Unexpected BoxFigure point count: %1", jsonPoints.count());
//
//    std::vector<Point> points;
//    for (int i = 0; i < jsonPoints.count(); ++i)
//    {
//        const auto point = get<QJsonArray>("$.figure.points", jsonPoints, i);
//        const QString path = NX_FMT("$.figure.points[%1]", i);
//        points.emplace_back(
//            get<double>(path, point, 0),
//            get<double>(path, point, 1));
//    }
//
//    Rect rect;
//    rect.x = points[0].x;
//    rect.y = points[0].y;
//    rect.width = points[1].x - points[0].x;
//    rect.height = points[1].y - points[0].y;
//
//    entry->emplaceValue(rect);
//}
//
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::IntrusionDetection::Rule::Inverted& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return entry.value() ? "true" : "false";
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::MissingObjectDetection::Rule::RequiresHumanInvolvement& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return entry.value() ? "true" : "false";
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::UnattendedObjectDetection::Rule::RequiresHumanInvolvement& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return entry.value() ? "true" : "false";
//}
//
//std::optional<QString> serializeEntryToServer(const CameraSettings::Entry<bool>& entry)
//{
//    if (!entry.hasValue())
//        return std::nullopt;
//
//    return entry.value() ? "true" : "false";
//}
//
//std::optional<QString> serializeEntryToServer(const CameraSettings::Vca::Sensitivity& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::Installation::Height& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::Installation::TiltAngle& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::Installation::RollAngle& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::CrowdDetection::Rule::SizeThreshold& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::CrowdDetection::Rule::EnterDelay& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::CrowdDetection::Rule::ExitDelay& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::LoiteringDetection::Rule::Delay& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::MissingObjectDetection::Rule::Delay& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::UnattendedObjectDetection::Rule::Delay& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::RunningDetection::Rule::MinCount& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::RunningDetection::Rule::MinSpeed& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(
//    const CameraSettings::Vca::RunningDetection::Rule::Delay& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(const CameraSettings::Entry<int>& entry)
//{
//    if (!entry.hasValue())
//        return std::nullopt;
//
//    return QString::number(entry.value());
//}
//
//std::optional<QString> serializeEntryToServer(const CameraSettings::Vca::RunningDetection::Rule::Name& entry)
//{
//    if (!entry.hasValue())
//        return "";
//
//    return entry.value();
//}
//
//QJsonObject serializeToServer(const NamedPointSequence* points)
//{
//    if (!points)
//    {
//        return QJsonObject{
//            {"label", ""},
//            {"figure", QJsonValue::Null},
//        };
//    }
//
//    return QJsonObject{
//        {"label", points->name},
//        {"figure", QJsonObject{
//            {"points",
//                [&]()
//                {
//                    QJsonArray jsonPoints;
//
//                    for (const auto& point: *points)
//                        jsonPoints.push_back(QJsonArray{point.x, point.y});
//
//                    return jsonPoints;
//                }(),
//            },
//        }},
//    };
//}
//
//std::optional<QString> serializeEntryToServer(const CameraSettings::Entry<NamedPolygon>& entry)
//{
//    const NamedPolygon* polygon = nullptr;
//    if (entry.hasValue())
//        polygon = &entry.value();
//
//    auto json = serializeToServer(polygon);
//
//    return serializeJson(json);
//}
//
//QString serializeToServer(NamedLine::Direction direction)
//{
//    switch (direction)
//    {
//        case NamedLine::Direction::any:
//            return "absent";
//        case NamedLine::Direction::leftToRight:
//            return "right";
//        case NamedLine::Direction::rightToLeft:
//            return "left";
//        default:
//            NX_ASSERT(false, "Unknown NamedLine::Direction value: %1", (int) direction);
//            return "";
//    }
//}
//
//std::optional<QString> serializeEntryToServer(const CameraSettings::Entry<NamedLine>& entry)
//{
//    const NamedLine* line = nullptr;
//    if (entry.hasValue())
//        line = &entry.value();
//
//    auto json = serializeToServer(line);
//
//    if (line)
//        set(&json, "figure", "direction", serializeToServer(line->direction));
//
//    return serializeJson(json);
//}
//
//std::optional<QString> serializeEntryToServer(const CameraSettings::Entry<Rect>& entry)
//{
//    if (!entry.hasValue())
//    {
//        return serializeJson(QJsonObject{
//            {"label", ""},
//            {"figure", QJsonValue::Null},
//        });
//    }
//
//    const auto& rect = entry.value();
//    return serializeJson(QJsonObject{
//        {"label", ""},
//        {"figure", QJsonObject{
//            {"points", QJsonArray{
//                QJsonArray{rect.x, rect.y},
//                QJsonArray{
//                    rect.x + rect.width,
//                    rect.y + rect.height,
//                },
//            }},
//        }},
//    });
//}
//
//
//QJsonValue getVcaInstallationModelForManifest()
//{
//    using Installation = CameraSettings::Vca::Installation;
//    return QJsonObject{
//        {"type", "GroupBox"},
//        {"caption", "Camera Installation"},
//        {"items", QJsonArray{
//            QJsonObject{
//                {"name", Installation::Height::name},
//                // Should really be SpinBox, but need empty state to work around camera not
//                // returning settings for disabled functionality.
//                {"type", "TextField"},
//                {"caption", "Height (cm)"},
//                {"description", "Distance between camera and floor"},
//            },
//            QJsonObject{
//                {"name", Installation::TiltAngle::name},
//                // Should really be SpinBox, but need empty state to work around camera not
//                // returning settings for disabled functionality.
//                {"type", "TextField"},
//                {"caption", "Tilt angle (°)"},
//                {"description", "Angle between camera direction axis and down direction"},
//            },
//            QJsonObject{
//                {"name", Installation::RollAngle::name},
//                // Should really be SpinBox, but need empty state to work around camera not
//                // returning settings for disabled functionality.
//                {"type", "TextField"},
//                {"caption", "Roll angle (°)"},
//                {"description", "Angle of camera rotation around direction axis"},
//            },
//            QJsonObject{
//                {"type", "Repeater"},
//                {"startIndex", 1},
//                {"count", (int) kMaxExclusionCount},
//                {"template", QJsonObject{
//                    {"name", Installation::Exclusion::Region::name},
//                    {"type", "PolygonFigure"},
//                    {"caption", "Exclusion region #"},
//                    {"minPoints", 3},
//                    {"maxPoints", 20}, // Defined in user guide.
//                }},
//            },
//        }},
//    };
//}
//
//QJsonValue getVcaCrowdDetectionModelForManifest()
//{
//    using Rule = CameraSettings::Vca::CrowdDetection::Rule;
//    return QJsonObject{
//        {"name", "Vca.CrowdDetection"},
//        {"type", "Section"},
//        {"caption", "Crowd Detection"},
//        {"items", QJsonArray{
//            QJsonObject{
//                {"type", "Repeater"},
//                {"startIndex", 1},
//                {"count", (int) kMaxDetectionRuleCount},
//                {"template", QJsonObject{
//                    {"type", "GroupBox"},
//                    {"caption", "Rule #"},
//                    {"filledCheckItems", QJsonArray{Rule::Region::name}},
//                    {"items", QJsonArray{
//                        QJsonObject{
//                            {"name", Rule::Region::name},
//                            {"type", "PolygonFigure"},
//                            {"caption", "Region"},
//                            {"minPoints", 3},
//                            {"maxPoints", 20}, // Defined in user guide.
//                        },
//                        QJsonObject{
//                            {"name", Rule::SizeThreshold::name},
//                            // Should really be SpinBox, but need empty state to work around camera not
//                            // returning settings for disabled functionality.
//                            {"type", "TextField"},
//                            {"caption", "Size threshold"},
//                            {"description", "At least this many people must be in the area to be considered a crowd"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::EnterDelay::name},
//                            // Should really be SpinBox, but need empty state to work around camera not
//                            // returning settings for disabled functionality.
//                            {"type", "TextField"},
//                            {"caption", "Entrance delay (s)"},
//                            {"description", "The event is only generated if the person stays in the area for at least this long"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::ExitDelay::name},
//                            // Should really be SpinBox, but need empty state to work around camera not
//                            // returning settings for disabled functionality.
//                            {"type", "TextField"},
//                            {"caption", "Exit delay (s)"},
//                            {"description", "The event is only generated if the person stays out of the area for at least this long"},
//                        },
//                    }},
//                }},
//            },
//        }},
//    };
//}
//
//QJsonValue getVcaLoiteringDetectionModelForManifest()
//{
//    using Rule = CameraSettings::Vca::LoiteringDetection::Rule;
//    return QJsonObject{
//        {"name", "Vca.LoiteringDetection"},
//        {"type", "Section"},
//        {"caption", "Loitering Detection"},
//        {"items", QJsonArray{
//            QJsonObject{
//                {"type", "Repeater"},
//                {"startIndex", 1},
//                {"count", (int) kMaxDetectionRuleCount},
//                {"template", QJsonObject{
//                    {"type", "GroupBox"},
//                    {"caption", "Rule #"},
//                    {"filledCheckItems", QJsonArray{Rule::Region::name}},
//                    {"items", QJsonArray{
//                        QJsonObject{
//                            {"name", Rule::Region::name},
//                            {"type", "PolygonFigure"},
//                            {"caption", "Region"},
//                            {"minPoints", 3},
//                            {"maxPoints", 20}, // Defined in user guide.
//                        },
//                        QJsonObject{
//                            {"name", Rule::Delay::name},
//                            // Should really be SpinBox, but need empty state to work around camera not
//                            // returning settings for disabled functionality.
//                            {"type", "TextField"},
//                            {"caption", "Delay (s)"},
//                            {"description", "The event is only generated if a person stays in the area for at least this long"},
//                        },
//                    }},
//                }},
//            },
//        }},
//    };
//}
//
//QJsonValue getVcaIntrusionDetectionModelForManifest()
//{
//    using Rule = CameraSettings::Vca::IntrusionDetection::Rule;
//    return QJsonObject{
//        {"name", "Vca.IntrusionDetection"},
//        {"type", "Section"},
//        {"caption", "Intrusion Detection"},
//        {"items", QJsonArray{
//            QJsonObject{
//                {"type", "Repeater"},
//                {"startIndex", 1},
//                {"count", (int) kMaxDetectionRuleCount},
//                {"template", QJsonObject{
//                    {"type", "GroupBox"},
//                    {"caption", "Rule #"},
//                    {"filledCheckItems", QJsonArray{Rule::Region::name}},
//                    {"items", QJsonArray{
//                        QJsonObject{
//                            {"name", Rule::Region::name},
//                            {"type", "PolygonFigure"},
//                            {"caption", "Region"},
//                            {"minPoints", 3},
//                            {"maxPoints", 20}, // Defined in user guide.
//                        },
//                        QJsonObject{
//                            {"name", Rule::Inverted::name},
//                            {"type", "ComboBox"},
//                            {"caption", "Direction"},
//                            {"description", "Movement direction which causes the event"},
//                            {"range", QJsonArray{
//                                // Need empty state to work around camera not returning settings
//                                // for disabled functionality.
//                                "",
//                                "false",
//                                "true",
//                            }},
//                            {"itemCaptions", QJsonObject{
//                                {"", ""},
//                                {"false", "In"},
//                                {"true", "Out"},
//                            }},
//                        },
//                    }},
//                }},
//            },
//        }},
//    };
//}
//
//QJsonValue getVcaLineCrossingDetectionModelForManifest()
//{
//    using Rule = CameraSettings::Vca::LineCrossingDetection::Rule;
//    return QJsonObject{
//        {"name", "Vca.LineCrossingDetection"},
//        {"type", "Section"},
//        {"caption", "Line Crossing Detection"},
//        {"items", QJsonArray{
//            QJsonObject{
//                {"type", "Repeater"},
//                {"startIndex", 1},
//                {"count", (int) kMaxDetectionRuleCount},
//                {"template", QJsonObject{
//                    {"type", "GroupBox"},
//                    {"caption", "Rule #"},
//                    {"filledCheckItems", QJsonArray{Rule::Line::name}},
//                    {"items", QJsonArray{
//                        QJsonObject{
//                            {"name", Rule::Line::name},
//                            {"type", "LineFigure"},
//                            {"caption", "Line"},
//                            {"minPoints", 3},
//                            {"maxPoints", 3},
//                        },
//                    }},
//                }},
//            },
//        }},
//    };
//}
//
//QJsonValue getVcaMissingObjectDetectionModelForManifest()
//{
//    using Rule = CameraSettings::Vca::MissingObjectDetection::Rule;
//    return QJsonObject{
//        {"name", "Vca.MissingObjectDetection"},
//        {"type", "Section"},
//        {"caption", "Missing Object Detection"},
//        {"items", QJsonArray{
//            QJsonObject{
//                {"type", "Repeater"},
//                {"startIndex", 1},
//                {"count", (int) kMaxDetectionRuleCount},
//                {"template", QJsonObject{
//                    {"type", "GroupBox"},
//                    {"caption", "Rule #"},
//                    {"filledCheckItems", QJsonArray{Rule::Region::name}},
//                    {"items", QJsonArray{
//                        QJsonObject{
//                            {"name", Rule::Region::name},
//                            {"type", "PolygonFigure"},
//                            {"caption", "Region"},
//                            {"minPoints", 3},
//                            {"maxPoints", 20}, // Defined in user guide.
//                        },
//                        // THe following two BoxFigures should really be single
//                        // ObjectSizeConstraints, but need empty state to work around camera not
//                        // returning settings for disabled functionality.
//                        QJsonObject{
//                            {"name", Rule::MinSize::name},
//                            {"type", "BoxFigure"},
//                            {"caption", "Minimum size"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::MaxSize::name},
//                            {"type", "BoxFigure"},
//                            {"caption", "Maximum size"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::Delay::name},
//                            // Should really be SpinBox, but need empty state to work around camera not
//                            // returning settings for disabled functionality.
//                            {"type", "TextField"},
//                            {"caption", "Delay (s)"},
//                            {"description", "The event is only generated if an object is missing for at least this long"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::RequiresHumanInvolvement::name},
//                            {"type", "ComboBox"},
//                            {"caption", "Human involvement"},
//                            {"description", "Whether a human is required to walk by an object prior to it going missing"},
//                            {"range", QJsonArray{
//                                // Need empty state to work around camera not returning settings
//                                // for disabled functionality.
//                                "",
//                                "false",
//                                "true",
//                            }},
//                            {"itemCaptions", QJsonObject{
//                                {"", ""},
//                                {"false", "Not required"},
//                                {"true", "Required"},
//                            }},
//                        },
//                    }},
//                }},
//            },
//        }},
//    };
//}
//
//QJsonValue getVcaUnattendedObjectDetectionModelForManifest()
//{
//    using Rule = CameraSettings::Vca::UnattendedObjectDetection::Rule;
//    return QJsonObject{
//        {"name", "Vca.UnattendedObjectDetection"},
//        {"type", "Section"},
//        {"caption", "Unattended Object Detection"},
//        {"items", QJsonArray{
//            QJsonObject{
//                {"type", "Repeater"},
//                {"startIndex", 1},
//                {"count", (int) kMaxDetectionRuleCount},
//                {"template", QJsonObject{
//                    {"type", "GroupBox"},
//                    {"caption", "Rule #"},
//                    {"filledCheckItems", QJsonArray{Rule::Region::name}},
//                    {"items", QJsonArray{
//                        QJsonObject{
//                            {"name", Rule::Region::name},
//                            {"type", "PolygonFigure"},
//                            {"caption", "Region"},
//                            {"minPoints", 3},
//                            {"maxPoints", 20}, // Defined in user guide.
//                        },
//                        // THe following two BoxFigures should really be single
//                        // ObjectSizeConstraints, but need empty state to work around camera not
//                        // returning settings for disabled functionality.
//                        QJsonObject{
//                            {"name", Rule::MinSize::name},
//                            {"type", "BoxFigure"},
//                            {"caption", "Minimum size"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::MaxSize::name},
//                            {"type", "BoxFigure"},
//                            {"caption", "Maximum size"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::Delay::name},
//                            // Should really be SpinBox, but need empty state to work around camera not
//                            // returning settings for disabled functionality.
//                            {"type", "TextField"},
//                            {"caption", "Delay (s)"},
//                            {"description", "The event is only generated if an object is unattended for at least this long"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::RequiresHumanInvolvement::name},
//                            {"type", "ComboBox"},
//                            {"caption", "Human involvement"},
//                            {"description", "Whether a human is required to walk by an object prior to it becoming unattended"},
//                            {"range", QJsonArray{
//                                // Need empty state to work around camera not returning settings
//                                // for disabled functionality.
//                                "",
//                                "false",
//                                "true",
//                            }},
//                            {"itemCaptions", QJsonObject{
//                                {"", ""},
//                                {"false", "Not required"},
//                                {"true", "Required"},
//                            }},
//                        },
//                    }},
//                }},
//            },
//        }},
//    };
//}
//
//QJsonValue getVcaFaceDetectionModelForManifest()
//{
//    using Rule = CameraSettings::Vca::FaceDetection::Rule;
//    return QJsonObject{
//        {"name", "Vca.FaceDetection"},
//        {"type", "Section"},
//        {"caption", "Face Detection"},
//        {"items", QJsonArray{
//            QJsonObject{
//                {"type", "Repeater"},
//                {"startIndex", 1},
//                {"count", (int) kMaxDetectionRuleCount},
//                {"template", QJsonObject{
//                    {"type", "GroupBox"},
//                    {"caption", "Rule #"},
//                    {"filledCheckItems", QJsonArray{Rule::Region::name}},
//                    {"items", QJsonArray{
//                        QJsonObject{
//                            {"name", Rule::Region::name},
//                            {"type", "PolygonFigure"},
//                            {"caption", "Region"},
//                            {"minPoints", 3},
//                            {"maxPoints", 20}, // Defined in user guide.
//                        },
//                    }},
//                }},
//            },
//        }},
//    };
//}
//
//QJsonValue getVcaRunningDetectionModelForManifest()
//{
//    using Rule = CameraSettings::Vca::RunningDetection::Rule;
//    return QJsonObject{
//        {"name", "Vca.RunningDetection"},
//        {"type", "Section"},
//        {"caption", "Running Detection"},
//        {"items", QJsonArray{
//            QJsonObject{
//                {"type", "Repeater"},
//                {"startIndex", 1},
//                {"count", (int) kMaxDetectionRuleCount},
//                {"template", QJsonObject{
//                    {"type", "GroupBox"},
//                    {"caption", "Rule #"},
//                    {"filledCheckItems", QJsonArray{Rule::Name::name}},
//                    {"items", QJsonArray{
//                        QJsonObject{
//                            {"name", Rule::Name::name},
//                            // Should really be SpinBox, but need empty state to work around camera not
//                            // returning settings for disabled functionality.
//                            {"type", "TextField"},
//                            {"caption", "Name"},
//                            {"description", "Name of this rule"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::MinCount::name},
//                            // Should really be SpinBox, but need empty state to work around camera not
//                            // returning settings for disabled functionality.
//                            {"type", "TextField"},
//                            {"caption", "Minimum runner count"},
//                            {"description", "The event is only generated if there is at least this many people running simultaneously"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::MinSpeed::name},
//                            // Should really be SpinBox, but need empty state to work around camera not
//                            // returning settings for disabled functionality.
//                            {"type", "TextField"},
//                            {"caption", "Minimum runnining speed"},
//                            {"description", "The event is only generated if people run at least this fast"},
//                        },
//                        QJsonObject{
//                            {"name", Rule::Delay::name},
//                            // Should really be SpinBox, but need empty state to work around camera not
//                            // returning settings for disabled functionality.
//                            {"type", "TextField"},
//                            {"caption", "Delay (s)"},
//                            {"description", "The event is only generated if people run for at least this long"},
//                        },
//                    }},
//                }},
//            },
//        }},
//    };
//}
//
//QJsonValue getVcaModelForManifest(const CameraFeatures::Vca& features)
//{
//    using Vca = CameraSettings::Vca;
//    return QJsonObject{
//        {"name", "Vca"},
//        {"type", "Section"},
//        {"caption", "Deep Learning VCA"},
//        {"items", QJsonArray{
//            QJsonObject{
//                {"name", Vca::Enabled::name},
//                {"type", "SwitchButton"},
//                {"caption", "Enabled"},
//            },
//            QJsonObject{
//                {"name", Vca::Sensitivity::name},
//                // Should really be SpinBox, but need empty state to work around camera not
//                // returning settings for disabled functionality.
//                {"type", "TextField"}, 
//                {"caption", "Sensitivity"},
//                {"description", "The higher the value the more likely an object will be detected as human"},
//            },
//            getVcaInstallationModelForManifest(),
//        }},
//        {"sections",
//            [&]()
//            {
//                QJsonArray sections;
//
//                if (features.crowdDetection)
//                    sections.push_back(getVcaCrowdDetectionModelForManifest());
//                if (features.loiteringDetection)
//                    sections.push_back(getVcaLoiteringDetectionModelForManifest());
//                if (features.intrusionDetection)
//                    sections.push_back(getVcaIntrusionDetectionModelForManifest());
//                if (features.lineCrossingDetection)
//                    sections.push_back(getVcaLineCrossingDetectionModelForManifest());
//                if (features.missingObjectDetection)
//                    sections.push_back(getVcaMissingObjectDetectionModelForManifest());
//                if (features.unattendedObjectDetection)
//                    sections.push_back(getVcaUnattendedObjectDetectionModelForManifest());
//                if (features.faceDetection)
//                    sections.push_back(getVcaFaceDetectionModelForManifest());
//                if (features.runningDetection)
//                    sections.push_back(getVcaRunningDetectionModelForManifest());
//
//                return sections;
//            }(),
//        },
//    };
//}



struct VcaParserFromCamera
{
    using Vca = CameraSettings::Vca;

    JsonArray functions;
    JsonObject analyticEngineConfig;
    JsonObject ruleEngineConfig;

    void parse(std::vector<Point>* value, const JsonValue& json) const
    {
        const auto subField = json.to<JsonArray>();
        value->resize(subField.count());
        for (int i = 0; i < subField.count(); ++i)
        {
            (*value)[i] = CameraVcaParameterApi::parsePoint(
                subField[i], subField.path);
        }
    }

    void parse(Polygon* value, const JsonValue& json) const
    {
        parse(static_cast<std::vector<Point>*>(value), json);
    }

    void parse(Vca::IntrusionDetection::Direction* value, const JsonValue& json) const
    {
        const auto serializedValue = json.to<QString>();
        if (serializedValue == "OutToIn")
            *value = Vca::IntrusionDetection::Direction::outToIn;
        else if (serializedValue == "InToOut")
            *value = Vca::IntrusionDetection::Direction::inToOut;
        else
            throw Exception("Failed to parse intrusion detection direction: %1", serializedValue);
    }

    template <typename Value>
    void parse(Value* value, const JsonValue& json) const
    {
        json.to(value);
    }

    template <typename Value, typename... ExtraArgs>
    void parse(CameraSettings::Entry<Value>* entry, const JsonValue& json,
        const ExtraArgs&... extraArgs) const
    {
        try
        {
            parse(&entry->value.emplace(), json, extraArgs...);
        }
        catch (const std::exception& exception)
        {
            entry->value = std::nullopt;
            if (!entry->error)
                entry->error = exception.what();
        }
    }

    void parse(Vca::Installation* installation) const
    {
        auto& height = installation->height;
        parse(&height, analyticEngineConfig["CamHeight"]);
        if (height.value)
            *height.value /= 10;  // Convert millimeters to centimeters.

        parse(&installation->tiltAngle, analyticEngineConfig["TiltAngle"]);
        parse(&installation->rollAngle, analyticEngineConfig["RollAngle"]);

        if (const auto exclusiveArea = ruleEngineConfig["ExclusiveArea"];
            exclusiveArea.isObject())
        {
            auto& excludedRegions = installation->excludedRegions;

            const auto field = exclusiveArea["Field"].to<JsonArray>();
            for (std::size_t i = 0; i < excludedRegions.size(); ++i)
            {
                auto& excludedRegion = excludedRegions[i];
                if (i < (std::size_t) field.count())
                    parse(&excludedRegion, field[i]);
                else
                    excludedRegion.value = std::nullopt;
            }
        }
    }

    void parse(Vca::CrowdDetection::Rule* rule, const JsonValue& jsonRule) const
    {
        parse(&rule->region, jsonRule["Field"][0]);
        parse(&rule->minPersonCount, jsonRule["PeopleNumber"]);
        parse(&rule->entranceDelay, jsonRule["EnterDelay"]);
        parse(&rule->exitDelay, jsonRule["LeaveDelay"]);
    }

    void parse(Vca::LoiteringDetection::Rule* rule, const JsonValue& jsonRule) const
    {
        parse(&rule->region, jsonRule["Field"][0]);
        parse(&rule->minDuration, jsonRule["StayTime"]);
    }

    void parse(Vca::IntrusionDetection::Rule* rule, const JsonValue& jsonRule) const
    {
        parse(&rule->region, jsonRule["Field"][0]);
        parse(&rule->direction, jsonRule["WalkingDirection"]);
    }

    template <typename Detection>
    void parseDetection(Detection* detection, const QString& functionName) const
    {
        if (!ruleEngineConfig[functionName].isObject())
            return;

        const auto jsonRules = ruleEngineConfig[functionName].to<JsonObject>();
        auto& rules = detection->rules;

        std::size_t i = 0;
        for (const auto& ruleName: jsonRules.keys())
        {
            if (i >= rules.size())
                break;

            auto& rule = rules[i];

            rule.name = ruleName;
            parse(&rule, jsonRules[ruleName]);

            ++i;
        }
        while (i < rules.size())
            rules[i++] = {};
    }

    template <typename Detection>
    void parseDetection(std::optional<Detection>* detection, const QString& functionName) const
    {
        for (int i = 0; i < functions.count(); ++i)
        {
            if (functions[i].to<QString>().contains(functionName))
            {
                if (!*detection)
                    detection->emplace();
                parseDetection(&**detection, functionName);
                return;
            }
        }
        *detection = std::nullopt;
    }

    void parse(Vca* vca) const
    {
        parse(&vca->sensitivity, analyticEngineConfig["Sensitivity"]);

        parse(&vca->installation);

        parseDetection(&vca->crowdDetection, "CrowdDetection");
        parseDetection(&vca->loiteringDetection, "LoiteringDetection");
        parseDetection(&vca->intrusionDetection, "IntrusionDetection");
    }
};

void fetchFromCamera(CameraSettings::Vca* vca,
    const CameraModuleApi::ModuleInfo& moduleInfo, const Url& cameraUrl)
{
    vca->enabled.value = moduleInfo.enabled;
    if (!moduleInfo.enabled)
        return;

    CameraVcaParameterApi api(cameraUrl);

    VcaParserFromCamera parser;

    api.fetch("Functions").get().to(&parser.functions);
    api.fetch("Config/AE").get().to(&parser.analyticEngineConfig);
    api.fetch("Config/RE").get().to(&parser.ruleEngineConfig);

    parser.parse(vca);
}

void fetchFromCamera(std::optional<CameraSettings::Vca>* vca,
    const CameraModuleApi::ModuleInfo& moduleInfo, const Url& cameraUrl)
{
    if (moduleInfo.index == -1)
    {
        *vca = std::nullopt;
        return;
    }

    if (!*vca)
        vca->emplace();
    fetchFromCamera(&**vca, moduleInfo, cameraUrl);
}

void fetchFromCamera(CameraSettings* settings, const Url& cameraUrl)
{
    auto moduleInfos = CameraModuleApi(cameraUrl).fetchModuleInfos();

    fetchFromCamera(&settings->vca, moduleInfos["VCA"], cameraUrl);
}











struct VcaSerializerToCamera
{
    using Vca = CameraSettings::Vca;

    JsonArray functions;
    JsonObject analyticEngineConfig;
    JsonObject ruleEngineConfig;

    template <typename Json, typename Key>
    void serializeValue(Json* json, const Key& key, const std::vector<Point>& value)
    {
        JsonArray subField;
        for (const auto& point: value)
        {
            subField.push_back(CameraVcaParameterApi::serialize(point));
        }
        (*json)[key] = subField;
    }

    template <typename Json, typename Key>
    void serializeValue(Json* json, const Key& key, const Polygon& value)
    {
        serializeValue(json, key, static_cast<const std::vector<Point>&>(value));
    }

    template <typename Json, typename Key>
    void serializeValue(Json* json, const Key& key,
       Vca::IntrusionDetection::Direction value)
    {
        switch (value)
        {
            case Vca::IntrusionDetection::Direction::outToIn:
                (*json)[key] = "OutToIn";
                return;
            case Vca::IntrusionDetection::Direction::inToOut:
                (*json)[key] = "InToOut";
                return;
            default:
                NX_ASSERT(false, "Unknown intrusion detection direction value: %1", (int) value);
        }
    }

    template <typename Json, typename Key, typename Value>
    void serializeValue(Json* json, const Key& key, const Value& value)
    {
        (*json)[key] = value;
    }

    template <typename Json, typename Key, typename Value, typename... ExtraArgs>
    void serialize(Json* json, const Key& key,
        CameraSettings::Entry<Value>* entry, const ExtraArgs&... extraArgs)
    {
        if (!entry->value)
            return;

        try
        {
            serializeValue(json, key, *entry->value, extraArgs...);
        }
        catch (const std::exception& exception)
        {
            if (!entry->error)
                entry->error = exception.what();
        }
    }

    void serializeField(JsonObject* jsonRule, CameraSettings::Entry<Polygon>* entry)
    {
        JsonArray field;
        field.push_back(JsonValue::Null);
        serialize(&field, 0, entry);
        if (field[0].isArray())
            (*jsonRule)["Field"] = field;
    }

    void serialize(Vca::Installation* installation)
    {
        if (auto& height = installation->height; height.value)
        {
            try
            {
                // Convert centimeters to millimeters.
                serializeValue(&analyticEngineConfig, "CamHeight", *height.value * 10);
            }
            catch (const std::exception& exception)
            {
                if (!height.error)
                    height.error = exception.what();
            }
        }

        serialize(&analyticEngineConfig, "TiltAngle", &installation->tiltAngle);
        serialize(&analyticEngineConfig, "RollAngle", &installation->rollAngle);

        {
            auto& excludedRegions = installation->excludedRegions;

            JsonObject exclusiveArea;
            if (ruleEngineConfig["ExclusiveArea"].isObject())
                ruleEngineConfig["ExclusiveArea"].to(&exclusiveArea);

            QJsonArray field;
            field.push_back(JsonValue::Null);
            for (auto& region: excludedRegions)
            {
                serialize(&field, field.count() - 1, &region);
                if (field.last().isArray())
                    field.push_back(JsonValue::Null);
            }
            if (!field.isEmpty() && !field.last().isArray())
                field.pop_back();

            exclusiveArea["Field"] = field;
            ruleEngineConfig["ExclusiveArea"] = exclusiveArea;
        }
    }

    bool serialize(JsonObject* jsonRule, Vca::CrowdDetection::Rule* rule)
    {
        if (!rule->region.value)
            return false;

        serializeField(jsonRule, &rule->region);
        serialize(jsonRule, "PeopleNumber", &rule->minPersonCount);
        serialize(jsonRule, "EnterDelay", &rule->entranceDelay);
        serialize(jsonRule, "LeaveDelay", &rule->exitDelay);
        
        return true;
    }

    bool serialize(JsonObject* jsonRule, Vca::LoiteringDetection::Rule* rule)
    {
        if (!rule->region.value)
            return false;

        serializeField(jsonRule, &rule->region);
        serialize(jsonRule, "StayTime", &rule->minDuration);
        
        return true;
    }

    bool serialize(JsonObject* jsonRule, Vca::IntrusionDetection::Rule* rule)
    {
        if (!rule->region.value)
            return false;

        serializeField(jsonRule, &rule->region);
        serialize(jsonRule, "WalkingDirection", &rule->direction);
        
        return true;
    }

    template <typename Detection>
    void serializeDetection(Detection* detection, const QString& functionName)
    {
        auto& rules = detection->rules;

        JsonObject jsonRules;
        if (ruleEngineConfig[functionName].isObject())
            ruleEngineConfig[functionName].to(&jsonRules);

        for (const auto& ruleName: jsonRules.keys())
        {
            if (std::none_of(rules.begin(), rules.end(),
                [&](const auto& rule)
                {
                    return rule.name == ruleName;
                }))
            {
                jsonRules.remove(ruleName);
            }
        }

        for (auto& rule: rules)
        {
            JsonObject jsonRule;
            if (jsonRules.contains(rule.name) && jsonRules[rule.name].isObject())
                jsonRules[rule.name].to(&jsonRule);

            jsonRule["RuleName"] = rule.name;
            jsonRule["EventName"] = rule.name;

            jsonRule.remove("Field3D");

            if (serialize(&jsonRule, &rule))
                jsonRules[rule.name] = jsonRule;
            else
                jsonRules.remove(rule.name);
        }

        if (jsonRules.isEmpty())
            ruleEngineConfig.remove(functionName);
        else
            ruleEngineConfig[functionName] = jsonRules;
    }

    template <typename Detection>
    void serializeDetection(std::optional<Detection>* detection, const QString& functionName)
    {
        if (!*detection)
            return;

        for (int i = 0; i < functions.count(); ++i)
        {
            if (functions[i].to<QString>().contains(functionName))
            {
                serializeDetection(&**detection, functionName);
                return;
            }
        }
    }

    void serialize(Vca* vca)
    {
        serialize(&analyticEngineConfig, "Sensitivity", &vca->sensitivity);

        serialize(&vca->installation);

        serializeDetection(&vca->crowdDetection, "CrowdDetection");
        serializeDetection(&vca->loiteringDetection, "LoiteringDetection");
        serializeDetection(&vca->intrusionDetection, "IntrusionDetection");
    }
};

void storeToCamera(const Url& cameraUrl,
    CameraSettings::Vca* vca, const CameraModuleApi::ModuleInfo& moduleInfo)
{
    if (auto& enabled = vca->enabled; enabled.value)
    {
        try
        {
            CameraModuleApi(cameraUrl).enable("VCA", *enabled.value);
            if (!*enabled.value || (!moduleInfo.enabled && *enabled.value))
                return;
        }
        catch (const std::exception& exception)
        {
            if (!enabled.error)
                enabled.error = exception.what();
            return;
        }
    }
    else
        return;

    CameraVcaParameterApi api(cameraUrl);

    VcaSerializerToCamera serializer;

    api.fetch("Functions").get().to(&serializer.functions);
    api.fetch("Config/AE").get().to(&serializer.analyticEngineConfig);
    api.fetch("Config/RE").get().to(&serializer.ruleEngineConfig);

    serializer.serialize(vca);

    api.store("Config/AE", serializer.analyticEngineConfig).get();
    api.store("Config/RE", serializer.ruleEngineConfig).get();
    api.reloadConfig().get();
}

void storeToCamera(const Url& cameraUrl,
    std::optional<CameraSettings::Vca>* vca, const CameraModuleApi::ModuleInfo& moduleInfo)
{
    if (moduleInfo.index == -1)
        return;

    if (*vca)
        storeToCamera(cameraUrl, &**vca, moduleInfo);
}

void storeToCamera(const Url& cameraUrl, CameraSettings* settings)
{
    auto moduleInfos = CameraModuleApi(cameraUrl).fetchModuleInfos();

    storeToCamera(cameraUrl, &settings->vca, moduleInfos["VCA"]);
}










const int kMaxExcludedRegionCount = 50; // Arbitrary big value.
const int kMaxDetectionRuleCount = 5; // Defined in user manual.

const int kMinRegionVertices = 3;
const int kMaxRegionVertices = 20; // Defined in user manual.

const QString kVcaEnabled = "Vca.Enabled";
const QString kVcaSensitivity = "Vca.Sensitivity";

const QString kVcaInstallationHeight = "Vca.Installation.Height";
const QString kVcaInstallationTiltAngle = "Vca.Installation.TiltAngle";
const QString kVcaInstallationRollAngle = "Vca.Installation.RollAngle";
const QString kVcaInstallationExcludedRegion = "Vca.Installation.ExcludedRegion#";

const QString kVcaCrowdDetectionRegion = "Vca.CrowdDetection#.Region";
const QString kVcaCrowdDetectionMinPersonCount = "Vca.CrowdDetection#.MinPersonCount";
const QString kVcaCrowdDetectionEntranceDelay = "Vca.CrowdDetection#.EntranceDelay";
const QString kVcaCrowdDetectionExitDelay = "Vca.CrowdDetection#.ExitDelay";

const QString kVcaLoiteringDetectionRegion = "Vca.LoiteringDetection#.Region";
const QString kVcaLoiteringDetectionMinDuration = "Vca.LoiteringDetection#.MinDuration";

const QString kVcaIntrusionDetectionRegion = "Vca.IntrusionDetection#.Region";
const QString kVcaIntrusionDetectionDirection = "Vca.IntrusionDetection#.Direction";

QString replicateName(QString name, std::size_t i)
{
    name.replace("#", "%1");
    return name.arg(i + 1);
}









struct ParserFromServer
{
    const IStringMap& values;

    std::map<QString, std::vector<std::function<void(const QString)>>> errorSetterGroups;

    template <typename Value>
    void noteName(const QString& name, CameraSettings::Entry<Value> *entry)
    {
        if (!entry->value)
            return;

        errorSetterGroups[name].push_back(
            [entry](const QString& error)
            {
                entry->value = std::nullopt;
                entry->error = error;
            });
    }

    void parse(bool* value, const QString& serializedValue)
    {
        if (serializedValue == "false")
            *value = false;
        else if (serializedValue == "true")
            *value = true;
        else
            throw Exception("Failed to parse boolean: %1", serializedValue);
    }

    void parse(int* value, const QString& serializedValue)
    {
        *value = toInt(serializedValue);
    }

    void parse(CameraSettings::Vca::IntrusionDetection::Direction* value,
        const QString& serializedValue)
    {
        if (serializedValue == "OutToIn")
            *value = CameraSettings::Vca::IntrusionDetection::Direction::outToIn;
        else if (serializedValue == "InToOut")
            *value = CameraSettings::Vca::IntrusionDetection::Direction::inToOut;
        else
            throw Exception("Failed to parse intrusion detection direction: %1", serializedValue);
    }

    void parse(std::optional<Polygon>* value, const QString& serializedValue, QString* label)
    {
        const auto json = parseJson(serializedValue.toUtf8()).to<JsonObject>();
        if (json.isEmpty())
            return;

        if (const auto figure = json["figure"]; figure.isObject())
        {
            value->emplace();
            const auto points = figure["points"].to<JsonArray>();
            for (int i = 0; i < points.count(); ++i)
            {
                const auto point = points[i];
                (*value)->emplace_back(
                    point.at(0).to<double>(),
                    point.at(1).to<double>());
            }

            if (label)
            {
                json["label"].to(label);
                if (label->isEmpty())
                    throw Exception("Empty name");
            }
        }
    }

    template <typename Value, typename... ExtraArgs>
    void parse(std::optional<Value>* entry, const QString& serializedValue,
        const ExtraArgs&... extraArgs)
    {
        parse(&entry->emplace(), serializedValue, extraArgs...);
    }

    template <typename Value, typename... ExtraArgs>
    void parse(CameraSettings::Entry<Value>* entry, const QString& name,
        const ExtraArgs&... extraArgs)
    {
        try
        {
            if (const char* const serializedValue = values.value(name.toStdString().data()))
                parse(&entry->value, serializedValue, extraArgs...);
        }
        catch (const std::exception& exception)
        {
            entry->value = std::nullopt;
            entry->error = exception.what();
        }
    }

    void parse(CameraSettings::Vca::Installation* installation)
    {
        parse(&installation->height, kVcaInstallationHeight);
        parse(&installation->tiltAngle, kVcaInstallationTiltAngle);
        parse(&installation->rollAngle, kVcaInstallationRollAngle);

        auto& excludedRegions = installation->excludedRegions;
        for (std::size_t i = 0; i < excludedRegions.size(); ++i)
        {
            parse(&excludedRegions[i], replicateName(kVcaInstallationExcludedRegion, i),
                static_cast<QString*>(nullptr));
        }
    }

    void parse(CameraSettings::Vca::CrowdDetection* detection)
    {
        auto& rules = detection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];

            parse(&rule.region, replicateName(kVcaCrowdDetectionRegion, i), &rule.name);
            parse(&rule.minPersonCount, replicateName(kVcaCrowdDetectionMinPersonCount, i));
            parse(&rule.entranceDelay, replicateName(kVcaCrowdDetectionEntranceDelay, i));
            parse(&rule.exitDelay, replicateName(kVcaCrowdDetectionExitDelay, i));

            noteName(rule.name, &rule.region);
        }
    }

    void parse(CameraSettings::Vca::LoiteringDetection* detection)
    {
        auto& rules = detection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];

            parse(&rule.region, replicateName(kVcaLoiteringDetectionRegion, i), &rule.name);
            parse(&rule.minDuration, replicateName(kVcaLoiteringDetectionMinDuration, i));

            noteName(rule.name, &rule.region);
        }
    }

    void parse(CameraSettings::Vca::IntrusionDetection* detection)
    {
        auto& rules = detection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];

            parse(&rule.region, replicateName(kVcaIntrusionDetectionRegion, i), &rule.name);
            parse(&rule.direction, replicateName(kVcaIntrusionDetectionDirection, i));

            noteName(rule.name, &rule.region);
        }
    }

    void parse(CameraSettings::Vca* vca)
    {
        parse(&vca->enabled, kVcaEnabled);
        parse(&vca->sensitivity, kVcaSensitivity);
        parse(&vca->installation);

        errorSetterGroups.clear();

        parse(&vca->crowdDetection.emplace());
        parse(&vca->loiteringDetection.emplace());
        parse(&vca->intrusionDetection.emplace());

        for (auto& [name, errorSetters]: errorSetterGroups)
        {
            if (errorSetters.size() == 1)
                continue;

            const QString error = NX_FMT("Duplicate name: %1", name);
            for (const auto setError: errorSetters)
                setError(error);
        }
    }

    void parse(CameraSettings* settings)
    {
        parse(&settings->vca.emplace());
    }
};

void parseFromServer(CameraSettings* settings, const IStringMap& values)
{
    ParserFromServer{values}.parse(settings);
}













struct SerializerToServer
{
    StringMap& values;
    StringMap& errors;

    QString serialize(bool value) const
    {
        return value ? "true" : "false";
    }

    QString serialize(int value) const
    {
        return QString::number(value);
    }

    QString serialize(CameraSettings::Vca::IntrusionDetection::Direction value) const
    {
        switch (value)
        {
            case CameraSettings::Vca::IntrusionDetection::Direction::outToIn:
                return "OutToIn";
            case CameraSettings::Vca::IntrusionDetection::Direction::inToOut:
                return "InToOut";
            default:
                NX_ASSERT(false, "Unknown intrusion detection direction value: %1", (int) value);
                return "";
        }
    }

    std::optional<QString> serialize(
        const std::optional<Polygon>& value, const QString& label) const
    {
        return serializeJson(QJsonObject{
            {"label", label},
            {"figure",
                [&]() -> QJsonValue
                {
                    if (!value)
                        return QJsonValue::Null;

                    QJsonArray points;
                    for (const auto& point: *value)
                        points.push_back(QJsonArray{point.x, point.y});

                    return QJsonObject{
                        {"points", points},
                    };
                }(),
            },
        });
    }

    template <typename Value, typename... ExtraArgs>
    std::optional<QString> serialize(
        const std::optional<Value>& value, ExtraArgs&&... extraArgs) const
    {
        if (!value)
            return std::nullopt;

        return serialize(*value, extraArgs...);
    }

    template <typename Value, typename... ExtraArgs>
    void serialize(const QString& name, const CameraSettings::Entry<Value>& entry,
        ExtraArgs&&... extraArgs) const
    {
        std::optional<QString> error;
        try
        {
            if (const auto serializedValue =
                serialize(entry.value, std::forward<ExtraArgs>(extraArgs)...))
            {
                values.setItem(name.toStdString(), serializedValue->toStdString());
            }
        }
        catch (const std::exception& exception)
        {
            error.emplace(exception.what());
        }

        if (entry.error)
            error = entry.error;

        if (error)
            errors.setItem(name.toStdString(), error->toStdString());
    }

    void serialize(const CameraSettings::Vca::Installation& installation) const
    {
        serialize(kVcaInstallationHeight, installation.height);
        serialize(kVcaInstallationTiltAngle, installation.tiltAngle);
        serialize(kVcaInstallationRollAngle, installation.rollAngle);

        const auto& excludedRegions = installation.excludedRegions;
        for (std::size_t i = 0; i < excludedRegions.size(); ++i)
            serialize(replicateName(kVcaInstallationExcludedRegion, i), excludedRegions[i], "");
    }

    void serialize(const CameraSettings::Vca::CrowdDetection& detection) const
    {
        const auto& rules = detection.rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            const auto& rule = rules[i];

            serialize(replicateName(kVcaCrowdDetectionRegion, i), rule.region, rule.name);
            serialize(replicateName(kVcaCrowdDetectionMinPersonCount, i), rule.minPersonCount);
            serialize(replicateName(kVcaCrowdDetectionEntranceDelay, i), rule.entranceDelay);
            serialize(replicateName(kVcaCrowdDetectionExitDelay, i), rule.exitDelay);
        }
    }

    void serialize(const CameraSettings::Vca::LoiteringDetection& detection) const
    {
        const auto& rules = detection.rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            const auto& rule = rules[i];

            serialize(replicateName(kVcaLoiteringDetectionRegion, i), rule.region, rule.name);
            serialize(replicateName(kVcaLoiteringDetectionMinDuration, i), rule.minDuration);
        }
    }

    void serialize(const CameraSettings::Vca::IntrusionDetection& detection) const
    {
        const auto& rules = detection.rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            const auto& rule = rules[i];

            serialize(replicateName(kVcaIntrusionDetectionRegion, i), rule.region, rule.name);
            serialize(replicateName(kVcaIntrusionDetectionDirection, i), rule.direction);
        }
    }

    void serialize(const CameraSettings::Vca& vca) const
    {
        serialize(kVcaEnabled, vca.enabled);
        serialize(kVcaSensitivity, vca.sensitivity);
        serialize(vca.installation);

        if (const auto& detection = vca.crowdDetection)
            serialize(*detection);
        if (const auto& detection = vca.loiteringDetection)
            serialize(*detection);
        if (const auto& detection = vca.intrusionDetection)
            serialize(*detection);
    }

    void serialize(const CameraSettings& settings) const
    {
        if (const auto& vca = settings.vca)
            serialize(*vca);
    }
};

void serializeToServer(SettingsResponse* response, const CameraSettings& settings)
{
    auto values = makePtr<StringMap>();
    auto errors = makePtr<StringMap>();

    SerializerToServer{*values, *errors}.serialize(settings);

    response->setValues(std::move(values));
    response->setErrors(std::move(errors));
}


QJsonObject serializeModel(const CameraSettings::Vca::Installation& installation)
{
    return QJsonObject{
        {"type", "GroupBox"},
        {"caption", "Camera Installation"},
        {"items", QJsonArray{
            QJsonObject{
                {"name", kVcaInstallationHeight},
                {"type", "SpinBox"},
                {"minValue", 0},
                {"maxValue", 2000},
                {"caption", "Height (cm)"},
                {"description", "Distance between camera and floor"},
            },
            QJsonObject{
                {"name", kVcaInstallationTiltAngle},
                {"type", "SpinBox"},
                {"minValue", 0},
                {"maxValue", 179},
                {"caption", "Tilt angle (°)"},
                {"description", "Angle between camera direction axis and down direction"},
            },
            QJsonObject{
                {"name", kVcaInstallationRollAngle},
                {"type", "SpinBox"},
                {"minValue", -74},
                {"maxValue", +74},
                {"caption", "Roll angle (°)"},
                {"description", "Angle of camera rotation around direction axis"},
            },
            QJsonObject{
                {"type", "GroupBox"},
                {"caption", "Excluded Regions"},
                {"items", QJsonArray{
                    QJsonObject{
                        {"type", "Repeater"},
                        {"startIndex", 1},
                        {"count", (int) installation.excludedRegions.size()},
                        {"template", QJsonObject{
                            {"name", kVcaInstallationExcludedRegion},
                            {"type", "PolygonFigure"},
                            {"minPoints", kMinRegionVertices},
                            {"maxPoints", kMaxRegionVertices},
                            {"caption", "#"},
                        }},
                    },
                }},
            },
        }},
    };
}

QJsonObject serializeModel(const CameraSettings::Vca::CrowdDetection& detection)
{
    return QJsonObject{
        {"name", "Vca.CrowdDetection"},
        {"type", "Section"},
        {"caption", "Crowd Detection"},
        {"items", QJsonArray{
            QJsonObject{
                {"type", "Repeater"},
                {"startIndex", 1},
                {"count", (int) detection.rules.size()},
                {"template", QJsonObject{
                    {"type", "GroupBox"},
                    {"caption", "#"},
                    {"filledCheckItems", QJsonArray{kVcaCrowdDetectionRegion}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", kVcaCrowdDetectionRegion},
                            {"type", "PolygonFigure"},
                            {"minPoints", kMinRegionVertices},
                            {"maxPoints", kMaxRegionVertices},
                        },
                        QJsonObject{
                            {"name", kVcaCrowdDetectionMinPersonCount},
                            {"type", "SpinBox"},
                            {"minValue", 0},
                            {"maxValue", 20},
                            {"defaultValue", 2},
                            {"caption", "Minimum person count"},
                            {"description", "At least this many people are required be considered"
                                            " a crowd"},
                        },
                        QJsonObject{
                            {"name", kVcaCrowdDetectionEntranceDelay},
                            {"type", "SpinBox"},
                            {"minValue", 0},
                            {"maxValue", 999},
                            {"caption", "Entrance delay (s)"},
                            {"description", "The event only starts if there are enough people"
                                            " staying in the region for at least this long"},
                        },
                        QJsonObject{
                            {"name", kVcaCrowdDetectionExitDelay},
                            {"type", "SpinBox"},
                            {"minValue", 0},
                            {"maxValue", 999},
                            {"caption", "Exit delay (s)"},
                            {"description", "The event only stops if there aren't enough people"
                                            "staying in the region for at least this long"},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonObject serializeModel(const CameraSettings::Vca::LoiteringDetection& detection)
{
    return QJsonObject{
        {"name", "Vca.LoiteringDetection"},
        {"type", "Section"},
        {"caption", "Loitering Detection"},
        {"items", QJsonArray{
            QJsonObject{
                {"type", "Repeater"},
                {"startIndex", 1},
                {"count", (int) detection.rules.size()},
                {"template", QJsonObject{
                    {"type", "GroupBox"},
                    {"caption", "Rule #"},
                    {"filledCheckItems", QJsonArray{kVcaLoiteringDetectionRegion}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", kVcaLoiteringDetectionRegion},
                            {"type", "PolygonFigure"},
                            {"caption", "Region"},
                            {"minPoints", kMinRegionVertices},
                            {"maxPoints", kMaxRegionVertices},
                        },
                        QJsonObject{
                            {"name", kVcaLoiteringDetectionMinDuration},
                            {"type", "SpinBox"},
                            {"minValue", 0},
                            {"maxValue", 999},
                            {"defaultValue", 5},
                            {"caption", "Minimum duration (s)"},
                            {"description", "The event is only generated if a person stays in the"
                                            " area for at least this long"},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonObject serializeModel(const CameraSettings::Vca::IntrusionDetection& detection)
{
    return QJsonObject{
        {"name", "Vca.IntrusionDetection"},
        {"type", "Section"},
        {"caption", "Intrusion Detection"},
        {"items", QJsonArray{
            QJsonObject{
                {"type", "Repeater"},
                {"startIndex", 1},
                {"count", (int) detection.rules.size()},
                {"template", QJsonObject{
                    {"type", "GroupBox"},
                    {"caption", "Rule #"},
                    {"filledCheckItems", QJsonArray{kVcaIntrusionDetectionRegion}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", kVcaIntrusionDetectionRegion},
                            {"type", "PolygonFigure"},
                            {"caption", "Region"},
                            {"minPoints", kMinRegionVertices},
                            {"maxPoints", kMaxRegionVertices},
                        },
                        QJsonObject{
                            {"name", kVcaIntrusionDetectionDirection},
                            {"type", "ComboBox"},
                            {"caption", "Direction"},
                            {"description", "Movement direction which causes the event"},
                            {"range", QJsonArray{
                                "OutToIn",
                                "InToOut",
                            }},
                            {"itemCaptions", QJsonObject{
                                {"OutToIn", "From outside to inside"},
                                {"InToOut", "From inside to outside"},
                            }},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonObject serializeModel(const CameraSettings::Vca& vca)
{
    return QJsonObject{
        {"name", "Vca"},
        {"type", "Section"},
        {"caption", "Deep Learning VCA"},
        {"items",
            [&]()
            {
                QJsonArray items;

                items.push_back(QJsonObject{
                    {"name", kVcaEnabled},
                    {"type", "SwitchButton"},
                    {"caption", "Enabled"},
                });
                if (vca.enabled.value.value_or(false))
                {
                    items.push_back(QJsonObject{
                        {"name", kVcaSensitivity},
                        {"type", "SpinBox"}, 
                        {"minValue", 0}, 
                        {"maxValue", 10}, 
                        {"defaultValue", 5}, 
                        {"caption", "Sensitivity"},
                        {"description", "Higher values correspond to greater likelihood of an"
                                        " object being detected as human"},
                    });
                    items.push_back(serializeModel(vca.installation));
                }

                return items;
            }(),
        },
        {"sections",
            [&]()
            {
                QJsonArray sections;

                if (const auto& detection = vca.crowdDetection)
                    sections.push_back(serializeModel(*detection));
                if (const auto& detection = vca.loiteringDetection)
                    sections.push_back(serializeModel(*detection));
                if (const auto& detection = vca.intrusionDetection)
                    sections.push_back(serializeModel(*detection));

                //serializeModel(vca ? vca->lineCrossingDetection : std::nullopt),
                //serializeModel(vca ? vca->missingObjectDetection : std::nullopt),
                //serializeModel(vca ? vca->unattendedObjectDetection : std::nullopt),
                //serializeModel(vca ? vca->faceDetection : std::nullopt),
                //serializeModel(vca ? vca->runningDetection : std::nullopt),

                return sections;
            }(),
        },
    };
}

QJsonObject serializeModel(const CameraSettings& settings)
{
    return QJsonObject{
        {"type", "Settings"},
        {"sections",
            [&]()
            {
                QJsonArray sections;

                if (const auto& vca = settings.vca)
                    sections.push_back(serializeModel(*vca));

                return sections;
            }(),
        },
    };
}

void serializeModel(SettingsResponse* response, const CameraSettings& settings)
{
    response->setModel(serializeJson(serializeModel(settings)).toStdString());
}

} //namespace

CameraSettings::Vca::Installation::Installation():
    excludedRegions(kMaxExcludedRegionCount)
{
}

CameraSettings::Vca::CrowdDetection::CrowdDetection():
    rules(kMaxDetectionRuleCount)
{
}

CameraSettings::Vca::LoiteringDetection::LoiteringDetection():
    rules(kMaxDetectionRuleCount)
{
}

CameraSettings::Vca::IntrusionDetection::IntrusionDetection():
    rules(kMaxDetectionRuleCount)
{
}

//CameraSettings::CameraSettings(const CameraFeatures& features)
//{
//    if (features.vca)
//    {
//        vca.emplace();
//
//        vca->installation.exclusions.resize(kMaxExclusionCount);
//
//        if (features.vca->crowdDetection)
//        {
//            auto& crowdDetection = vca->crowdDetection.emplace();
//            crowdDetection.rules.resize(kMaxDetectionRuleCount);
//        }
//        if (features.vca->loiteringDetection)
//        {
//            auto& loiteringDetection = vca->loiteringDetection.emplace();
//            loiteringDetection.rules.resize(kMaxDetectionRuleCount);
//        }
//        if (features.vca->intrusionDetection)
//        {
//            auto& intrusionDetection = vca->intrusionDetection.emplace();
//            intrusionDetection.rules.resize(kMaxDetectionRuleCount);
//        }
//        if (features.vca->lineCrossingDetection)
//        {
//            auto& lineCrossingDetection = vca->lineCrossingDetection.emplace();
//            lineCrossingDetection.rules.resize(kMaxDetectionRuleCount);
//        }
//        if (features.vca->missingObjectDetection)
//        {
//            auto& missingObjectDetection = vca->missingObjectDetection.emplace();
//            missingObjectDetection.rules.resize(kMaxDetectionRuleCount);
//        }
//        if (features.vca->unattendedObjectDetection)
//        {
//            auto& unattendedObjectDetection = vca->unattendedObjectDetection.emplace();
//            unattendedObjectDetection.rules.resize(kMaxDetectionRuleCount);
//        }
//        if (features.vca->faceDetection)
//        {
//            auto& faceDetection = vca->faceDetection.emplace();
//            faceDetection.rules.resize(kMaxDetectionRuleCount);
//        }
//        if (features.vca->runningDetection)
//        {
//            auto& runningDetection = vca->runningDetection.emplace();
//            runningDetection.rules.resize(kMaxDetectionRuleCount);
//        }
//    }
//}

void CameraSettings::fetchFrom(const Url& cameraUrl)
{
    fetchFromCamera(this, cameraUrl);
}

void CameraSettings::storeTo(const Url& cameraUrl)
{
    storeToCamera(cameraUrl, this);
}

void CameraSettings::parseFromServer(const IStringMap& values)
{
    vivotek::parseFromServer(this, values);
}

Ptr<SettingsResponse> CameraSettings::serializeToServer() const
{
    auto response = makePtr<SettingsResponse>();

    vivotek::serializeToServer(response.get(), *this);
    serializeModel(response.get(), *this);

    return response;
}

} // namespace nx::vms_server_plugins::analytics::vivotek
