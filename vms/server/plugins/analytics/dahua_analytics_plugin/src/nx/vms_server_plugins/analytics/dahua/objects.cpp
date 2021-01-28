#include "objects.h"

#include <map>

#include <QtCore/QStringView>
#include <QtCore/QJsonArray>

namespace nx::vms_server_plugins::analytics::dahua {

using namespace nx::sdk;

namespace {

bool isValidStringTrafficAttributeValue(const QJsonValue& jsonValue) {
    if (!jsonValue.isString())
        return false;

    const auto value = jsonValue.toString();
    // "unknow" is not a typo, trailing "n" is missing intentionally.
    return value != "" && !value.toLower().startsWith("unknow");
}

void parseHumanAttributes(std::vector<Attribute>* attributes, const QJsonObject& object)
{
    if (const auto value = object["Stature"]; value.isDouble())
    {
        attributes->emplace_back(
            "Height (cm)", QString::number(value.toInt()), Attribute::Type::number);
    }
}

void parseVehicleAttributes(std::vector<Attribute>* attributes, const QJsonObject& object)
{
    constexpr auto& isValidStringValue = isValidStringTrafficAttributeValue;

    if (const auto value = object["Category"]; isValidStringValue(value))
        attributes->emplace_back("Category", value.toString());

    if (const auto value = object["Text"]; isValidStringValue(value))
        attributes->emplace_back("Logo", value.toString());

    if (const auto value = object["Speed"]; value.isDouble())
    {
        attributes->emplace_back(
            "Speed (km/h)", QString::number(value.toDouble()), Attribute::Type::number);
    }

    if (const auto value = object["TrafficCar"]; value.isObject())
    {
        const auto trafficCar = value.toObject();

        if (const auto value = trafficCar["CarType"]; isValidStringValue(value))
            attributes->emplace_back("Type", value.toString());

        if (const auto value = trafficCar["VehicleColor"]; isValidStringValue(value))
            attributes->emplace_back("Color", value.toString());

        if (const auto value = trafficCar["VehicleSize"]; isValidStringValue(value))
            attributes->emplace_back("Size", value.toString());

        if (const auto value = trafficCar["PlateNumber"]; isValidStringValue(value))
            attributes->emplace_back("LicensePlateNumber", value.toString());

        if (const auto value = trafficCar["PlateType"]; isValidStringValue(value))
            attributes->emplace_back("LicensePlateType", value.toString());

        if (const auto value = trafficCar["PlateColor"]; isValidStringValue(value))
            attributes->emplace_back("LicensePlateColor", value.toString());

        if (const auto value = trafficCar["Country"]; isValidStringValue(value))
            attributes->emplace_back("LicensePlateCountry", value.toString());

        if (const auto value = trafficCar["Province"]; isValidStringValue(value))
            attributes->emplace_back("LicensePlateProvince", value.toString());

        if (const auto value = trafficCar["Lane"]; value.isDouble())
        {
            attributes->emplace_back(
                "Lane", QString::number(value.toDouble()), Attribute::Type::number);
        }
    }
}

void parsePlateAttributes(std::vector<Attribute>* attributes, const QJsonObject& object)
{
    constexpr auto& isValidStringValue = isValidStringTrafficAttributeValue;

    if (const auto value = object["Text"]; isValidStringValue(value))
        attributes->emplace_back("Number", value.toString());

    if (const auto value = object["TrafficCar"]; value.isObject())
    {
        const auto trafficCar = value.toObject();

        if (const auto value = trafficCar["PlateType"]; isValidStringValue(value))
            attributes->emplace_back("Type", value.toString());

        if (const auto value = trafficCar["PlateColor"]; isValidStringValue(value))
            attributes->emplace_back("Color", value.toString());
    }

    if (const auto value = object["Country"]; isValidStringValue(value))
        attributes->emplace_back("Country", value.toString());

    if (const auto value = object["Province"]; isValidStringValue(value))
        attributes->emplace_back("Province", value.toString());
}

void parseHumanFaceAttributes(std::vector<Attribute>* attributes, const QJsonObject& object)
{
    if (const auto value = object["Sex"]; value.isString())
        attributes->emplace_back("Sex", value.toString());

    if (const auto value = object["Age"]; value.isDouble())
    {
        attributes->emplace_back(
            "Age", QString::number(value.toInt()), Attribute::Type::number);
    }

    if (const auto value = object["Feature"]; value.isArray())
    {
        const auto features = value.toArray();

        if (features.contains("SunGlasses"))
        {
            attributes->emplace_back("WearsGlasses", "true", Attribute::Type::boolean);
            attributes->emplace_back("GlassesType", "Sunglasses");
        }
        else if (features.contains("WearGlasses"))
        {
            attributes->emplace_back("WearsGlasses", "true", Attribute::Type::boolean);
            attributes->emplace_back("GlassesType", "Regular");
        }
        else if (features.contains("NoGlasses"))
        {
            attributes->emplace_back("WearsGlasses", "false", Attribute::Type::boolean);
        }

        static const std::vector<QString> expressions = {
            "Smile",
            "Anger",
            "Sadness",
            "Disgust",
            "Fear",
            "Surprise",
            "Neutral",
            "Laugh",
            "Happy",
            "Confused",
            "Scream",
        };
        for (const auto& expression: expressions)
        {
            if (features.contains(expression))
            {
                attributes->emplace_back("Expression", expression);
                break;
            }
        }
    }

    if (const auto value = object["Eye"].toInt())
        attributes->emplace_back("Eyes", (value == 1) ? "Closed" : "Open");

    if (const auto value = object["Mouth"].toInt())
        attributes->emplace_back("Mouth", (value == 1) ? "Closed" : "Open");

    if (const auto value = object["Mask"].toInt())
    {
        attributes->emplace_back(
            "Mask", (value == 1) ? "false" : "true", Attribute::Type::boolean);
    }

    if (const auto value = object["Beard"].toInt())
    {
        attributes->emplace_back(
            "Beard", (value == 1) ? "false" : "true", Attribute::Type::boolean);
    }
}

} // namespace

static std::vector<const ObjectType*> objectTypes;

#define NX_DEFINE_OBJECT_TYPE(NAME) \
    namespace { \
    \
    struct NAME##ObjectType: ObjectType \
    { \
        NAME##ObjectType(); \
    }; \
    \
    } /* namespace */ \
    \
    const ObjectType ObjectType::k##NAME = \
        (objectTypes.push_back(&k##NAME), NAME##ObjectType()); \
    \
    NAME##ObjectType::NAME##ObjectType()

NX_DEFINE_OBJECT_TYPE(Unknown)
{
    nativeId = "Unknown";
    id = "nx.dahua.Unknown";
    prettyName = "Unknown";
}

NX_DEFINE_OBJECT_TYPE(Human)
{
    nativeId = "Human";
    id = "nx.dahua.Human";
    prettyName = "Human";
    parseAttributes = parseHumanAttributes;
}

NX_DEFINE_OBJECT_TYPE(NonMotor)
{
    nativeId = "NonMotor";
    id = "nx.dahua.NonMotor";
    prettyName = "Non-motorized vehicle";
}

NX_DEFINE_OBJECT_TYPE(Vehicle)
{
    nativeId = "Vehicle";
    id = "nx.dahua.Vehicle";
    prettyName = "Vehicle";
    parseAttributes = parseVehicleAttributes;
}

NX_DEFINE_OBJECT_TYPE(Plate)
{
    nativeId = "Plate";
    id = "nx.dahua.Plate";
    prettyName = "License plate";
    parseAttributes = parsePlateAttributes;
}

NX_DEFINE_OBJECT_TYPE(HumanFace)
{
    nativeId = "HumanFace";
    id = "nx.dahua.HumanFace";
    prettyName = "Human face";
    parseAttributes = parseHumanFaceAttributes;
}

#undef NX_DEFINE_OBJECT_TYPE

static const auto objectTypeByNativeId =
    []()
    {
        std::map<QStringView, const ObjectType*> map;

        for (const auto type: objectTypes)
            map[type->nativeId] = type;

        return map;
    }();

const ObjectType* ObjectType::findByNativeId(const QString& nativeId)
{
    if (const auto it = objectTypeByNativeId.find(nativeId); it != objectTypeByNativeId.end())
        return it->second;

    return nullptr;
}

const std::vector<const ObjectType*>& kObjectTypes = objectTypes;

} // namespace nx::vms_server_plugins::analytics::dahua
