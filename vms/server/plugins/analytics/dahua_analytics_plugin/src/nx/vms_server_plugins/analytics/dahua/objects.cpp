#include "objects.h"

#include <map>

#include <QtCore/QStringView>
#include <QtCore/QJsonArray>

namespace nx::vms_server_plugins::analytics::dahua {

using namespace nx::sdk;

namespace {

std::vector<ObjectAttribute> parseHumanFaceAttributes(const QJsonObject& object)
{
    std::vector<ObjectAttribute> attributes;

    if (const auto jsonValue = object["Sex"]; jsonValue.isString())
        attributes.push_back({"Sex", jsonValue.toString()});

    if (const auto jsonValue = object["Age"]; jsonValue.isDouble())
        attributes.push_back({"Age", QString::number(jsonValue.toInt()), Attribute::Type::number});

    if (const auto jsonValue = object["Feature"]; jsonValue.isArray())
    {
        const auto features = jsonValue.toArray();

        if (features.contains("SunGlasses"))
        {
            attributes.push_back({"WearsGlasses", "true", Attribute::Type::boolean});
            attributes.push_back({"GlassesType", "Sunglasses"});
        }
        else if (features.contains("WearGlasses"))
        {
            attributes.push_back({"WearsGlasses", "true", Attribute::Type::boolean});
            attributes.push_back({"GlassesType", "Regular"});
        }
        else if (features.contains("NoGlasses"))
        {
            attributes.push_back({"WearsGlasses", "false", Attribute::Type::boolean});
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
                attributes.push_back({"Expression", expression});
                break;
            }
        }
    }

    if (const auto value = object["Eye"].toInt())
        attributes.push_back({"Eyes", (value == 1) ? "Closed" : "Open"});

    if (const auto value = object["Mouth"].toInt())
        attributes.push_back({"Mouth", (value == 1) ? "Closed" : "Open"});

    if (const auto value = object["Mask"].toInt())
        attributes.push_back({"Mask", (value == 1) ? "false" : "true", Attribute::Type::boolean});

    if (const auto value = object["Beard"].toInt())
        attributes.push_back({"Beard", (value == 1) ? "false" : "true", Attribute::Type::boolean});

    return attributes;
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
