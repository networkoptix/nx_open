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
            (*value)[i] = CameraVcaParameterApi::parsePoint(subField[i]);
    }

    void parse(Polygon* value, const JsonValue& json) const
    {
        parse(static_cast<std::vector<Point>*>(value), json);
    }

    void parse(Line* value, const JsonValue& json) const
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

    void parse(Vca::LineCrossingDetection::Direction* value, const JsonValue& json) const
    {
        const auto serializedValue = json.to<QString>();
        if (serializedValue == "Any")
            *value = Vca::LineCrossingDetection::Direction::any;
        else if (serializedValue == "Out")
            *value = Vca::LineCrossingDetection::Direction::leftToRight;
        else if (serializedValue == "In")
            *value = Vca::LineCrossingDetection::Direction::rightToLeft;
        else
        {
            throw Exception("Failed to parse line corssing detection direction: %1",
                serializedValue);
        }
    }

    void parse(Rect* value, const JsonValue& json) const
    {
        value->x = CameraVcaParameterApi::parseCoordinate(json["x"]);
        value->y = CameraVcaParameterApi::parseCoordinate(json["y"]);
        value->width = CameraVcaParameterApi::parseCoordinate(json["w"]);
        value->height = CameraVcaParameterApi::parseCoordinate(json["h"]);
    }

    void parse(SizeConstraints* value, const JsonValue& json) const
    {
        parse(&value->min, json["Minimum"]);
        parse(&value->max, json["Maximum"]);
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

    void setRuleName(Vca::RunningDetection::Rule *rule, const QString& name) const
    {
        rule->name.value = name;
        rule->name.error = std::nullopt;
    }

    template <typename Rule>
    void setRuleName(Rule *rule, const QString& name) const
    {
        rule->name = name;
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

    void parse(Vca::LineCrossingDetection::Rule* rule, const JsonValue& jsonRule) const
    {
        parse(&rule->line, jsonRule["Line"][0]);
        parse(&rule->direction, jsonRule["Direction"]);
    }

    void parse(Vca::MissingObjectDetection::Rule* rule, const JsonValue& jsonRule) const
    {
        parse(&rule->region, jsonRule["Field"][0]);
        parse(&rule->sizeConstraints, jsonRule["AreaLimitation"]);
        parse(&rule->minDuration, jsonRule["Duration"]);
        parse(&rule->requiresHumanInvolvement, jsonRule["HumanFactor"]);
    }

    void parse(Vca::UnattendedObjectDetection::Rule* rule, const JsonValue& jsonRule) const
    {
        parse(&rule->region, jsonRule["Field"][0]);
        parse(&rule->sizeConstraints, jsonRule["AreaLimitation"]);
        parse(&rule->minDuration, jsonRule["Duration"]);
        parse(&rule->requiresHumanInvolvement, jsonRule["HumanFactor"]);
    }

    void parse(Vca::FaceDetection::Rule* rule, const JsonValue& jsonRule) const
    {
        parse(&rule->region, jsonRule["Field"][0]);
    }

    void parse(Vca::RunningDetection::Rule* rule, const JsonValue& jsonRule) const
    {
        parse(&rule->minPersonCount, jsonRule["PeopleNumber"]);
        parse(&rule->minSpeed, jsonRule["SpeedLevel"]);
        parse(&rule->minDuration, jsonRule["MinActivityDuration"]);
        parse(&rule->maxMergeInterval, jsonRule["ActivityMergeInterval"]);
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

            setRuleName(&rule, ruleName);
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
        parseDetection(&vca->lineCrossingDetection, "LineCrossingDetection");
        parseDetection(&vca->missingObjectDetection, "MissingObjectDetection");
        parseDetection(&vca->unattendedObjectDetection, "UnattendedObjectDetection");
        parseDetection(&vca->faceDetection, "FaceDetection");
        parseDetection(&vca->runningDetection, "RunningDetection");
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

///////////////////////////////////////////////////////////////////////////////////////////////////

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
    void serializeValue(Json* json, const Key& key, const Line& value)
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

    template <typename Json, typename Key>
    void serializeValue(Json* json, const Key& key,
       Vca::LineCrossingDetection::Direction value)
    {
        switch (value)
        {
            case Vca::LineCrossingDetection::Direction::any:
                (*json)[key] = "Any";
                return;
            case Vca::LineCrossingDetection::Direction::leftToRight:
                (*json)[key] = "Out";
                return;
            case Vca::LineCrossingDetection::Direction::rightToLeft:
                (*json)[key] = "In";
                return;
            default:
                NX_ASSERT(false, "Unknown line crossing detection direction value: %1",
                    (int) value);
        }
    }

    template <typename Json, typename Key>
    void serializeValue(Json* json, const Key& key, const Rect& value)
    {
        (*json)[key] = JsonObject{
            {"x", CameraVcaParameterApi::serializeCoordinate(value.x)},
            {"y", CameraVcaParameterApi::serializeCoordinate(value.y)},
            {"w", CameraVcaParameterApi::serializeCoordinate(value.width)},
            {"h", CameraVcaParameterApi::serializeCoordinate(value.height)},
        };
    }

    template <typename Json, typename Key>
    void serializeValue(Json* json, const Key& key, const SizeConstraints& value)
    {
        JsonObject areaLimitation;
        
        serializeValue(&areaLimitation, "Minimum", value.min);
        serializeValue(&areaLimitation, "Maximum", value.max);

        (*json)[key] = areaLimitation;
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
        {
            (*jsonRule)["Field"] = field;
            jsonRule->remove("Field3D");
        }
    }

    void serializeLine(JsonObject* jsonRule, CameraSettings::Entry<Line>* entry)
    {
        JsonArray line;
        line.push_back(JsonValue::Null);
        serialize(&line, 0, entry);
        if (line[0].isArray())
        {
            (*jsonRule)["Line"] = line;
            jsonRule->remove("Line3D");
        }
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

    std::optional<QString> getRuleName(const Vca::LineCrossingDetection::Rule& rule)
    {
        if (!rule.line.value)
            return std::nullopt;

        return rule.name;
    }

    std::optional<QString> getRuleName(const Vca::RunningDetection::Rule& rule)
    {
        if (!rule.name.value || rule.name.value->isEmpty())
            return std::nullopt;

        return rule.name.value;
    }

    template <typename Rule>
    std::optional<QString> getRuleName(const Rule& rule)
    {
        if (!rule.region.value)
            return std::nullopt;

        return rule.name;
    }

    void serialize(JsonObject* jsonRule, Vca::CrowdDetection::Rule* rule)
    {
        serializeField(jsonRule, &rule->region);
        serialize(jsonRule, "PeopleNumber", &rule->minPersonCount);
        serialize(jsonRule, "EnterDelay", &rule->entranceDelay);
        serialize(jsonRule, "LeaveDelay", &rule->exitDelay);
    }

    void serialize(JsonObject* jsonRule, Vca::LoiteringDetection::Rule* rule)
    {
        serializeField(jsonRule, &rule->region);
        serialize(jsonRule, "StayTime", &rule->minDuration);
    }

    void serialize(JsonObject* jsonRule, Vca::IntrusionDetection::Rule* rule)
    {
        serializeField(jsonRule, &rule->region);
        serialize(jsonRule, "WalkingDirection", &rule->direction);
    }

    void serialize(JsonObject* jsonRule, Vca::LineCrossingDetection::Rule* rule)
    {
        serializeLine(jsonRule, &rule->line);
        serialize(jsonRule, "Direction", &rule->direction);
    }

    void serialize(JsonObject* jsonRule, Vca::MissingObjectDetection::Rule* rule)
    {
        serializeField(jsonRule, &rule->region);
        serialize(jsonRule, "AreaLimitation", &rule->sizeConstraints);
        serialize(jsonRule, "Duration", &rule->minDuration);
        serialize(jsonRule, "HumanFactor", &rule->requiresHumanInvolvement);
    }

    void serialize(JsonObject* jsonRule, Vca::UnattendedObjectDetection::Rule* rule)
    {
        serializeField(jsonRule, &rule->region);
        serialize(jsonRule, "AreaLimitation", &rule->sizeConstraints);
        serialize(jsonRule, "Duration", &rule->minDuration);
        serialize(jsonRule, "HumanFactor", &rule->requiresHumanInvolvement);
    }

    void serialize(JsonObject* jsonRule, Vca::FaceDetection::Rule* rule)
    {
        serializeField(jsonRule, &rule->region);
    }

    void serialize(JsonObject* jsonRule, Vca::RunningDetection::Rule* rule)
    {
        serialize(jsonRule, "PeopleNumber", &rule->minPersonCount);
        serialize(jsonRule, "SpeedLevel", &rule->minSpeed);
        serialize(jsonRule, "MinActivityDuration", &rule->minDuration);
        serialize(jsonRule, "ActivityMergeInterval", &rule->maxMergeInterval);
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
                    return getRuleName(rule) == ruleName;
                }))
            {
                jsonRules.remove(ruleName);
            }
        }

        for (auto& rule: rules)
        {
            const auto ruleName = getRuleName(rule);
            if (!ruleName)
                continue;

            JsonObject jsonRule;
            if (jsonRules.contains(*ruleName) && jsonRules[*ruleName].isObject())
                jsonRules[*ruleName].to(&jsonRule);

            jsonRule["RuleName"] = *ruleName;
            jsonRule["EventName"] = *ruleName;

            serialize(&jsonRule, &rule);

            jsonRules[*ruleName] = jsonRule;
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
        serializeDetection(&vca->lineCrossingDetection, "LineCrossingDetection");
        serializeDetection(&vca->missingObjectDetection, "MissingObjectDetection");
        serializeDetection(&vca->unattendedObjectDetection, "UnattendedObjectDetection");
        serializeDetection(&vca->faceDetection, "FaceDetection");
        serializeDetection(&vca->runningDetection, "RunningDetection");
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

///////////////////////////////////////////////////////////////////////////////////////////////////

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

const QString kVcaLineCrossingDetectionLine = "Vca.LineCrossingDetection#.Line";

const QString kVcaMissingObjectDetectionRegion = "Vca.MissingObjectDetection#.Region";
const QString kVcaMissingObjectDetectionSizeConstraints =
    "Vca.MissingObjectDetection#.SizeConstraints";
const QString kVcaMissingObjectDetectionMinDuration = "Vca.MissingObjectDetection#.MinDuration";
const QString kVcaMissingObjectDetectionRequiresHumanInvolvement =
    "Vca.MissingObjectDetection#.RequiresHumanInvolvement";

const QString kVcaUnattendedObjectDetectionRegion = "Vca.UnattendedObjectDetection#.Region";
const QString kVcaUnattendedObjectDetectionSizeConstraints =
    "Vca.UnattendedObjectDetection#.SizeConstraints";
const QString kVcaUnattendedObjectDetectionMinDuration =
    "Vca.UnattendedObjectDetection#.MinDuration";
const QString kVcaUnattendedObjectDetectionRequiresHumanInvolvement =
    "Vca.UnattendedObjectDetection#.RequiresHumanInvolvement";

const QString kVcaFaceDetectionRegion = "Vca.FaceDetection#.Region";

const QString kVcaRunningDetectionEnabled = "Vca.RunningDetection#.Enabled";
const QString kVcaRunningDetectionName = "Vca.RunningDetection#.Name";
const QString kVcaRunningDetectionMinPersonCount = "Vca.RunningDetection#.MinPersonCount";
const QString kVcaRunningDetectionMinSpeed = "Vca.RunningDetection#.MinSpeed";
const QString kVcaRunningDetectionMinDuration = "Vca.RunningDetection#.MinDuration";
const QString kVcaRunningDetectionMaxMergeInterval = "Vca.RunningDetection#.MaxMergeInterval";

QString replicateName(QString name, std::size_t i)
{
    name.replace("#", "%1");
    return name.arg(i + 1);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class UniqueRuleRenamer
{
public:
    void reset()
    {
        m_names.clear();
    }

    void add(QString* name)
    {
        m_names.push_back(name);
    }

    void rename()
    {
        std::vector<ParsedName> parsedNames; 
        for (auto* name: m_names)
        {
            if (name->isEmpty())
                *name = "Rule-1";

            parsedNames.push_back(parseName(name));
        }

        std::map<QString, std::map<int, QString*>> occupiedIndices;
        for (const auto& [name, stem, index]: parsedNames)
            occupiedIndices[stem].emplace(index, name);

        for (auto& [name, stem, index]: parsedNames)
        {
            auto& occupied = occupiedIndices[stem];

            if (const auto it = occupied.find(index); it != occupied.end())
            {
                if (it->second == name)
                    continue;

                index = 1;
            }

            while (occupied.count(index))
                ++index;

            occupied.emplace(index, name);
        }

        for (const auto& parsedName: parsedNames)
            serialize(parsedName);
    }

private:
    struct ParsedName
    {
        QString* name;
        QString stem;
        int index = -1;
    };

private:
    static ParsedName parseName(QString* name)
    {
        int dashIndex = name->lastIndexOf("-");
        if (dashIndex == -1)
            return ParsedName{name, *name};

        const auto indexString = name->midRef(dashIndex + 1);
        if (indexString.isEmpty())
            return ParsedName{name, *name};

        int index;
        if (bool ok; (index = indexString.toInt(&ok)), !ok)
            return ParsedName{name, *name};

        return ParsedName{name, name->left(dashIndex), index};
    }

    static void serialize(const ParsedName& parsedName)
    {
        if (parsedName.index == -1)
            *parsedName.name = parsedName.stem;
        else
            *parsedName.name = NX_FMT("%1-%2", parsedName.stem, parsedName.index);
    }

private:
    std::vector<QString*> m_names;
};

struct ParserFromServer
{
    const IStringMap& values;

    UniqueRuleRenamer renamer{};

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

    void parse(QString* value, const QString& serializedValue)
    {
        *value = serializedValue;
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

    void parse(CameraSettings::Vca::LineCrossingDetection::Direction* value,
        const QString& serializedValue)
    {
        if (serializedValue == "absent")
            *value = CameraSettings::Vca::LineCrossingDetection::Direction::any;
        else if (serializedValue == "right")
            *value = CameraSettings::Vca::LineCrossingDetection::Direction::leftToRight;
        else if (serializedValue == "left")
            *value = CameraSettings::Vca::LineCrossingDetection::Direction::rightToLeft;
        else
        {
            throw Exception("Failed to parse line crossing detection direction: %1",
                serializedValue);
        }
    }

    void parsePoints(std::vector<Point>* points, const JsonArray& jsonPoints)
    {
        points->clear();
        for (int i = 0; i < jsonPoints.count(); ++i)
        {
            const auto jsonPoint = jsonPoints[i];
            points->emplace_back(
                jsonPoint.at(0).to<double>(),
                jsonPoint.at(1).to<double>());
        }
    }

    void parse(std::optional<Polygon>* value, const QString& serializedValue, QString* label)
    {
        const auto json = parseJson(serializedValue.toUtf8()).to<JsonObject>();
        if (json.isEmpty())
            return;

        if (const auto figure = json["figure"]; figure.isObject())
        {
            parsePoints(&value->emplace(), figure["points"].to<JsonArray>());

            if (label)
                json["label"].to(label);
        }
    }

    void parse(std::optional<Line>* value, const QString& serializedValue, QString* label,
        CameraSettings::Entry<CameraSettings::Vca::LineCrossingDetection::Direction>* direction)
    {
        const auto json = parseJson(serializedValue.toUtf8()).to<JsonObject>();
        if (json.isEmpty())
            return;

        if (const auto figure = json["figure"]; figure.isObject())
        {
            parsePoints(&value->emplace(), figure["points"].to<JsonArray>());

            json["label"].to(label);

            try
            {
                parse(&direction->value.emplace(), figure["direction"].to<QString>());
            }
            catch (const std::exception& exception)
            {
                direction->value = std::nullopt;
                direction->error = exception.what();
            }
        }
    }

    void parse(std::optional<SizeConstraints>* value, const QString& serializedValue)
    {
        const auto json = parseJson(serializedValue.toUtf8()).to<JsonObject>();
        if (json.isEmpty())
            return;

        auto& [min, max] = value->emplace();

        json["positions"].at(0).at(0).to(&min.x);
        json["positions"].at(0).at(1).to(&min.y);
        json["minimum"].at(0).to(&min.width);
        json["minimum"].at(1).to(&min.height);

        json["positions"].at(1).at(0).to(&max.x);
        json["positions"].at(1).at(1).to(&max.y);
        json["maximum"].at(0).to(&max.width);
        json["maximum"].at(1).to(&max.height);
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

            if (rule.region.value)
                renamer.add(&rule.name);
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

            if (rule.region.value)
                renamer.add(&rule.name);
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

            if (rule.region.value)
                renamer.add(&rule.name);
        }
    }

    void parse(CameraSettings::Vca::LineCrossingDetection* detection)
    {
        auto& rules = detection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];

            parse(&rule.line, replicateName(kVcaLineCrossingDetectionLine, i),
                &rule.name, &rule.direction);

            if (rule.line.value)
                renamer.add(&rule.name);
        }
    }

    void parse(CameraSettings::Vca::MissingObjectDetection* detection)
    {
        auto& rules = detection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];

            parse(&rule.region, replicateName(kVcaMissingObjectDetectionRegion, i), &rule.name);
            parse(&rule.sizeConstraints,
                replicateName(kVcaMissingObjectDetectionSizeConstraints, i));
            parse(&rule.minDuration, replicateName(kVcaMissingObjectDetectionMinDuration, i));
            parse(&rule.requiresHumanInvolvement,
                replicateName(kVcaMissingObjectDetectionRequiresHumanInvolvement, i));

            if (rule.region.value)
                renamer.add(&rule.name);
        }
    }

    void parse(CameraSettings::Vca::UnattendedObjectDetection* detection)
    {
        auto& rules = detection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];

            parse(&rule.region,
                replicateName(kVcaUnattendedObjectDetectionRegion, i), &rule.name);
            parse(&rule.sizeConstraints,
                replicateName(kVcaUnattendedObjectDetectionSizeConstraints, i));
            parse(&rule.minDuration,
                replicateName(kVcaUnattendedObjectDetectionMinDuration, i));
            parse(&rule.requiresHumanInvolvement,
                replicateName(kVcaUnattendedObjectDetectionRequiresHumanInvolvement, i));

            if (rule.region.value)
                renamer.add(&rule.name);
        }
    }

    void parse(CameraSettings::Vca::FaceDetection* detection)
    {
        auto& rules = detection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];

            parse(&rule.region, replicateName(kVcaFaceDetectionRegion, i), &rule.name);

            if (rule.region.value)
                renamer.add(&rule.name);
        }
    }

    void parse(CameraSettings::Vca::RunningDetection* detection)
    {
        auto& rules = detection->rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            auto& rule = rules[i];

            CameraSettings::Entry<bool> enabled;

            parse(&enabled, replicateName(kVcaRunningDetectionEnabled, i));
            parse(&rule.name, replicateName(kVcaRunningDetectionName, i));
            parse(&rule.minPersonCount, replicateName(kVcaRunningDetectionMinPersonCount, i));
            parse(&rule.minSpeed, replicateName(kVcaRunningDetectionMinSpeed, i));
            parse(&rule.minDuration, replicateName(kVcaRunningDetectionMinDuration, i));
            parse(&rule.maxMergeInterval, replicateName(kVcaRunningDetectionMaxMergeInterval, i));

            if (enabled.value.value_or(false))
            {
                if (!rule.name.value)
                    rule.name.value = "";
                renamer.add(&*rule.name.value);
            }
            else
                rule.name.value = std::nullopt;
        }
    }

    void parse(CameraSettings::Vca* vca)
    {
        parse(&vca->enabled, kVcaEnabled);
        parse(&vca->sensitivity, kVcaSensitivity);
        parse(&vca->installation);

        renamer.reset();

        parse(&vca->crowdDetection.emplace());
        parse(&vca->loiteringDetection.emplace());
        parse(&vca->intrusionDetection.emplace());
        parse(&vca->lineCrossingDetection.emplace());
        parse(&vca->missingObjectDetection.emplace());
        parse(&vca->unattendedObjectDetection.emplace());
        parse(&vca->faceDetection.emplace());
        parse(&vca->runningDetection.emplace());

        renamer.rename();
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

///////////////////////////////////////////////////////////////////////////////////////////////////

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

    QString serialize(QString value) const
    {
        return value;
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

    QString serialize(CameraSettings::Vca::LineCrossingDetection::Direction value) const
    {
        switch (value)
        {
            case CameraSettings::Vca::LineCrossingDetection::Direction::any:
                return "absent";
            case CameraSettings::Vca::LineCrossingDetection::Direction::leftToRight:
                return "right";
            case CameraSettings::Vca::LineCrossingDetection::Direction::rightToLeft:
                return "left";
            default:
                NX_ASSERT(false, "Unknown line crossing detection direction value: %1", (int) value);
                return "";
        }
    }

    QString serialize(const SizeConstraints& value) const
    {
        return serializeJson(JsonObject{
            {"minimum", JsonArray{value.min.width, value.min.height}},
            {"maximum", JsonArray{value.max.width, value.max.height}},
            {"positions", JsonArray{
                JsonArray{value.min.x, value.min.y},
                JsonArray{value.max.x, value.max.y},
            }},
        });
    }

    JsonArray serializePoints(const std::vector<Point>& points) const
    {
        JsonArray jsonPoints;
        for (const auto& point: points)
            jsonPoints.push_back(JsonArray{point.x, point.y});

        return jsonPoints;
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

                    return QJsonObject{
                        {"points", serializePoints(*value)},
                    };
                }(),
            },
        });
    }

    std::optional<QString> serialize(
        const std::optional<Line>& value, const QString& label,
        const CameraSettings::Entry<CameraSettings::Vca::LineCrossingDetection::Direction>&
            direction) const
    {
        return serializeJson(QJsonObject{
            {"label", label},
            {"figure",
                [&]() -> QJsonValue
                {
                    if (!value)
                        return QJsonValue::Null;

                    if (direction.error)
                        throw Exception(*direction.error);

                    return QJsonObject{
                        {"direction", serialize(*direction.value)},
                        {"points", serializePoints(*value)},
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
            error = exception.what();
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

    void serialize(const CameraSettings::Vca::LineCrossingDetection& detection) const
    {
        const auto& rules = detection.rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            const auto& rule = rules[i];

            serialize(replicateName(kVcaLineCrossingDetectionLine, i),
                rule.line, rule.name, rule.direction);
        }
    }

    void serialize(const CameraSettings::Vca::MissingObjectDetection& detection) const
    {
        const auto& rules = detection.rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            const auto& rule = rules[i];

            serialize(replicateName(kVcaMissingObjectDetectionRegion, i), rule.region, rule.name);
            serialize(replicateName(kVcaMissingObjectDetectionSizeConstraints, i),
                rule.sizeConstraints);
            serialize(replicateName(kVcaMissingObjectDetectionMinDuration, i), rule.minDuration);
            serialize(replicateName(kVcaMissingObjectDetectionRequiresHumanInvolvement, i),
                rule.requiresHumanInvolvement);
        }
    }

    void serialize(const CameraSettings::Vca::UnattendedObjectDetection& detection) const
    {
        const auto& rules = detection.rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            const auto& rule = rules[i];

            serialize(replicateName(kVcaUnattendedObjectDetectionRegion, i),
                rule.region, rule.name);
            serialize(replicateName(kVcaUnattendedObjectDetectionSizeConstraints, i),
                rule.sizeConstraints);
            serialize(replicateName(kVcaUnattendedObjectDetectionMinDuration, i),
                rule.minDuration);
            serialize(replicateName(kVcaUnattendedObjectDetectionRequiresHumanInvolvement, i),
                rule.requiresHumanInvolvement);
        }
    }

    void serialize(const CameraSettings::Vca::FaceDetection& detection) const
    {
        const auto& rules = detection.rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            const auto& rule = rules[i];

            serialize(replicateName(kVcaFaceDetectionRegion, i), rule.region, rule.name);
        }
    }

    void serialize(const CameraSettings::Vca::RunningDetection& detection) const
    {
        const auto& rules = detection.rules;
        for (std::size_t i = 0; i < rules.size(); ++i)
        {
            const auto& rule = rules[i];

            auto name = rule.name;
            if (!name.value)
                name.value = "";

            CameraSettings::Entry<bool> enabled;
            enabled.value = name.value != "";

            serialize(replicateName(kVcaRunningDetectionEnabled, i), enabled);
            serialize(replicateName(kVcaRunningDetectionName, i), name);
            serialize(replicateName(kVcaRunningDetectionMinPersonCount, i), rule.minPersonCount);
            serialize(replicateName(kVcaRunningDetectionMinSpeed, i), rule.minSpeed);
            serialize(replicateName(kVcaRunningDetectionMinDuration, i), rule.minDuration);
            serialize(replicateName(kVcaRunningDetectionMaxMergeInterval, i), rule.maxMergeInterval);
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
        if (const auto& detection = vca.lineCrossingDetection)
            serialize(*detection);
        if (const auto& detection = vca.missingObjectDetection)
            serialize(*detection);
        if (const auto& detection = vca.unattendedObjectDetection)
            serialize(*detection);
        if (const auto& detection = vca.faceDetection)
            serialize(*detection);
        if (const auto& detection = vca.runningDetection)
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

///////////////////////////////////////////////////////////////////////////////////////////////////

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
                    {"caption", "Rule #"},
                    {"filledCheckItems", QJsonArray{kVcaCrowdDetectionRegion}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", kVcaCrowdDetectionRegion},
                            {"type", "PolygonFigure"},
                            {"caption", "Name"},
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
                            {"caption", "Name"},
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
                            {"caption", "Name"},
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

QJsonObject serializeModel(const CameraSettings::Vca::LineCrossingDetection& detection)
{
    return QJsonObject{
        {"name", "Vca.LineCrossingDetection"},
        {"type", "Section"},
        {"caption", "Line Crossing Detection"},
        {"items", QJsonArray{
            QJsonObject{
                {"type", "Repeater"},
                {"startIndex", 1},
                {"count", (int) detection.rules.size()},
                {"template", QJsonObject{
                    {"type", "GroupBox"},
                    {"caption", "Rule #"},
                    {"filledCheckItems", QJsonArray{kVcaLineCrossingDetectionLine}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", kVcaLineCrossingDetectionLine},
                            {"type", "LineFigure"},
                            {"caption", "Name"},
                            {"minPoints", 3},
                            {"maxPoints", 3},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonObject serializeModel(const CameraSettings::Vca::MissingObjectDetection& detection)
{
    return QJsonObject{
        {"name", "Vca.MissingObjectDetection"},
        {"type", "Section"},
        {"caption", "Missing Object Detection"},
        {"items", QJsonArray{
            QJsonObject{
                {"type", "Repeater"},
                {"startIndex", 1},
                {"count", (int) detection.rules.size()},
                {"template", QJsonObject{
                    {"type", "GroupBox"},
                    {"caption", "Rule #"},
                    {"filledCheckItems", QJsonArray{kVcaMissingObjectDetectionRegion}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", kVcaMissingObjectDetectionRegion},
                            {"type", "PolygonFigure"},
                            {"caption", "Name"},
                            {"minPoints", kMinRegionVertices},
                            {"maxPoints", kMaxRegionVertices},
                        },
                        QJsonObject{
                            {"name", kVcaMissingObjectDetectionSizeConstraints},
                            {"type", "ObjectSizeConstraints"},
                            {"caption", "Size constraints"},
                        },
                        QJsonObject{
                            {"name", kVcaMissingObjectDetectionMinDuration},
                            {"type", "SpinBox"},
                            {"minValue", 0},
                            {"maxValue", 999},
                            {"defaultValue", 300},
                            {"caption", "Minimum duration (s)"},
                            {"description", "The event is only generated if an object is missing"
                                            " for at least this long"},
                        },
                        QJsonObject{
                            {"name", kVcaMissingObjectDetectionRequiresHumanInvolvement},
                            {"type", "CheckBox"},
                            {"caption", "Human involvement"},
                            {"description", "Whether a human is required to walk by an object"
                                            " prior to it going missing"},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonObject serializeModel(const CameraSettings::Vca::UnattendedObjectDetection& detection)
{
    return QJsonObject{
        {"name", "Vca.UnattendedObjectDetection"},
        {"type", "Section"},
        {"caption", "Unattended Object Detection"},
        {"items", QJsonArray{
            QJsonObject{
                {"type", "Repeater"},
                {"startIndex", 1},
                {"count", (int) detection.rules.size()},
                {"template", QJsonObject{
                    {"type", "GroupBox"},
                    {"caption", "Rule #"},
                    {"filledCheckItems", QJsonArray{kVcaUnattendedObjectDetectionRegion}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", kVcaUnattendedObjectDetectionRegion},
                            {"type", "PolygonFigure"},
                            {"caption", "Name"},
                            {"minPoints", kMinRegionVertices},
                            {"maxPoints", kMaxRegionVertices},
                        },
                        QJsonObject{
                            {"name", kVcaUnattendedObjectDetectionSizeConstraints},
                            {"type", "ObjectSizeConstraints"},
                            {"caption", "Size constraints"},
                        },
                        QJsonObject{
                            {"name", kVcaUnattendedObjectDetectionMinDuration},
                            {"type", "SpinBox"},
                            {"minValue", 0},
                            {"maxValue", 999},
                            {"defaultValue", 300},
                            {"caption", "Minimum duration (s)"},
                            {"description", "The event is only generated if an object is unattended"
                                            " for at least this long"},
                        },
                        QJsonObject{
                            {"name", kVcaUnattendedObjectDetectionRequiresHumanInvolvement},
                            {"type", "CheckBox"},
                            {"caption", "Human involvement"},
                            {"description", "Whether a human is required to walk by an object"
                                            " prior to it going unattended"},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonObject serializeModel(const CameraSettings::Vca::FaceDetection& detection)
{
    return QJsonObject{
        {"name", "Vca.FaceDetection"},
        {"type", "Section"},
        {"caption", "Face Detection"},
        {"items", QJsonArray{
            QJsonObject{
                {"type", "Repeater"},
                {"startIndex", 1},
                {"count", (int) detection.rules.size()},
                {"template", QJsonObject{
                    {"type", "GroupBox"},
                    {"caption", "Rule #"},
                    {"filledCheckItems", QJsonArray{kVcaFaceDetectionRegion}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", kVcaFaceDetectionRegion},
                            {"type", "PolygonFigure"},
                            {"caption", "Name"},
                            {"minPoints", kMinRegionVertices},
                            {"maxPoints", kMaxRegionVertices},
                        },
                    }},
                }},
            },
        }},
    };
}

QJsonObject serializeModel(const CameraSettings::Vca::RunningDetection& detection)
{
    return QJsonObject{
        {"name", "Vca.RunningDetection"},
        {"type", "Section"},
        {"caption", "Running Detection"},
        {"items", QJsonArray{
            QJsonObject{
                {"type", "Repeater"},
                {"startIndex", 1},
                {"count", (int) detection.rules.size()},
                {"template", QJsonObject{
                    {"type", "GroupBox"},
                    {"caption", "Rule #"},
                    {"filledCheckItems", QJsonArray{kVcaRunningDetectionName}},
                    {"items", QJsonArray{
                        QJsonObject{
                            {"name", kVcaRunningDetectionEnabled},
                            {"type", "CheckBox"},
                            {"caption", "Enabled"},
                        },
                        QJsonObject{
                            {"name", kVcaRunningDetectionName},
                            {"type", "TextField"},
                            {"caption", "Name"},
                        },
                        QJsonObject{
                            {"name", kVcaRunningDetectionMinPersonCount},
                            {"type", "SpinBox"},
                            {"minValue", 1},
                            {"maxValue", 3},
                            {"caption", "Minimum person count"},
                            {"description", "The event is only generated if there is at least"
                                            " this many people running simultaneously"},
                        },
                        QJsonObject{
                            {"name", kVcaRunningDetectionMinSpeed},
                            {"type", "SpinBox"},
                            {"minValue", 1},
                            {"maxValue", 6},
                            {"defaultValue", 3},
                            {"caption", "Minimum speed"},
                            {"description", "The event is only generated if people run at least"
                                            " this fast"},
                        },
                        QJsonObject{
                            {"name", kVcaRunningDetectionMinDuration},
                            {"type", "SpinBox"},
                            {"minValue", 0},
                            {"maxValue", 999},
                            {"defaultValue", 1},
                            {"caption", "Minimum duration (s)"},
                            {"description", "The event is only generated if people are running"
                                            " run for at least this long"},
                        },
                        QJsonObject{
                            {"name", kVcaRunningDetectionMaxMergeInterval},
                            {"type", "SpinBox"},
                            {"minValue", 0},
                            {"maxValue", 999},
                            {"defaultValue", 1},
                            {"caption", "Max merge interval (s)"},
                            {"description", "Two consecutive running events are merged into one"
                                            " if the time between the end of the first one and"
                                            " the start of the second one is less than this"},
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
                if (const auto& detection = vca.lineCrossingDetection)
                    sections.push_back(serializeModel(*detection));
                if (const auto& detection = vca.missingObjectDetection)
                    sections.push_back(serializeModel(*detection));
                if (const auto& detection = vca.unattendedObjectDetection)
                    sections.push_back(serializeModel(*detection));
                if (const auto& detection = vca.faceDetection)
                    sections.push_back(serializeModel(*detection));
                if (const auto& detection = vca.runningDetection)
                    sections.push_back(serializeModel(*detection));

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

CameraSettings::Vca::LineCrossingDetection::LineCrossingDetection():
    rules(kMaxDetectionRuleCount)
{
}

CameraSettings::Vca::MissingObjectDetection::MissingObjectDetection():
    rules(kMaxDetectionRuleCount)
{
}

CameraSettings::Vca::UnattendedObjectDetection::UnattendedObjectDetection():
    rules(kMaxDetectionRuleCount)
{
}

CameraSettings::Vca::FaceDetection::FaceDetection():
    rules(kMaxDetectionRuleCount)
{
}

CameraSettings::Vca::RunningDetection::RunningDetection():
    rules(kMaxDetectionRuleCount)
{
}

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
