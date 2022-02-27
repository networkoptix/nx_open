// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "module.h"

#include "object_type.h"
#include "enum_type.h"
#include "color_type.h"
#include "attribute_generator.h"
#include "taxonomy_object.h"
#include "generators.h"

#include <nx/vms_server_plugins/analytics/stub/utils.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

static const std::string kNoBase{""};

static const std::string kStdColorsTypeId{"nx.base.Colors"};

static const std::string kStdLengthEnumTypeId{"nx.base.Length"};
static const std::string kStdHatTypeEnumTypeId{"nx.base.HatType"};
static const std::string kStdAgeEnumTypeId{"nx.base.Age"};
static const std::string kStdGenderEnumTypeId{"nx.base.Gender"};

static const std::string kStdVehicleTypeId{"nx.base.Vehicle"};
static const std::string kStdCarTypeId{"nx.base.Car"};
static const std::string kStdTruckTypeId{"nx.base.Truck"};
static const std::string kStdBusTypeId{"nx.base.Bus"};
static const std::string kStdPersonTypeId{"nx.base.Person"};
static const std::string kStdBagTypeId{"nx.base.Bag"};
static const std::string kStdHatTypeId{"nx.base.Hat"};
static const std::string kStdGlassesTypeId{"nx.base.Glasses"};
static const std::string kStdFaceTypeId{"nx.base.Face"};
static const std::string kStdLicensePlateTypeId{"nx.base.LicensePlate"};

static const std::set<std::string> kExcludedFromSupportedTypes = {
    kStdHatTypeId, kStdGlassesTypeId, kStdBagTypeId
};

static const std::map<std::string, ObjectType> kObjectTypesById = {
    {kStdVehicleTypeId, {
        kStdVehicleTypeId,
        "Vehicle",
        kStdVehicleTypeId,
        kNoBase,
        {
            {"Color", "Color", kStdColorsTypeId},
            {"Speed", "Number", "Integer", /*minValue*/ 0, /*maxValue*/ 300, "kph"},
            {"Make", "String"},
            {"Model", "String"},
            {"License Plate", "Object", kStdLicensePlateTypeId},
        }
    }},
    {kStdCarTypeId, {
        kStdCarTypeId,
        "Car",
        kStdCarTypeId,
        kStdVehicleTypeId,
        {}
    }},
    {kStdTruckTypeId, {
        kStdTruckTypeId,
        "Truck",
        kStdTruckTypeId,
        kStdVehicleTypeId,
        {}
    }},
    {kStdBusTypeId, {
        kStdBusTypeId,
        "Bus",
        kStdBusTypeId,
        kStdVehicleTypeId,
        {}
    }},
    {kStdPersonTypeId, {
        kStdPersonTypeId,
        "Person",
        kStdPersonTypeId,
        kNoBase,
        {
            {"Gender", "Enum", kStdGenderEnumTypeId},
            {"Hat", "Object", kStdHatTypeId},
            {"Glasses", "Object", kStdGlassesTypeId},
            {"Mask", "Boolean"},
            {"Bag", "Object", kStdBagTypeId},
            {"Top Clothing Color", "Color", kStdColorsTypeId},
            {"Top Clothing Length", "Enum", kStdLengthEnumTypeId},
            {"Bottom Clothing Color", "Color", kStdColorsTypeId},
            {"Bottom Clothing Length", "Enum", kStdLengthEnumTypeId},
        }
    }},
    {kStdHatTypeId, {
        kStdHatTypeId,
        "Hat",
        kStdHatTypeId,
        kNoBase,
        {
            {"Hat Type", "Enum", kStdHatTypeEnumTypeId},
        }
    }},
    {kStdBagTypeId, {
        kStdBagTypeId,
        "Bag",
        kStdBagTypeId,
        kNoBase,
        {}
    }},
    {kStdGlassesTypeId, {
        kStdGlassesTypeId,
        "Glasses",
        kStdGlassesTypeId,
        kNoBase,
        {}
    }},
    {kStdFaceTypeId, {
        kStdFaceTypeId,
        "Face",
        kStdFaceTypeId,
        kNoBase,
        {
            // TODO: other attributes
            {"Hair Color", "Color", kStdColorsTypeId},
            {"Hair Length", "Enum", kStdLengthEnumTypeId},
            {"Eye Color", "Color", kStdColorsTypeId},
        }
    }},
    {kStdLicensePlateTypeId, {
        kStdLicensePlateTypeId,
        "License Plate",
        kStdLicensePlateTypeId,
        kNoBase,
        {
            {"Number", "String"}
        }
    }}
};

static const std::map<std::string, EnumType> kEnumTypesById = {
    {kStdLengthEnumTypeId, {
        kStdLengthEnumTypeId,
        "Length",
        {"Short", "Medium", "Long"}
    }},
    {kStdHatTypeEnumTypeId, {
        kStdHatTypeEnumTypeId,
        "Hat Type",
        {"Short", "Long"}
    }},
    {kStdAgeEnumTypeId, {
        kStdAgeEnumTypeId,
        "Age",
        {"Young", "Middle", "Adult", "Senior"}
    }},
    {kStdGenderEnumTypeId, {
        kStdGenderEnumTypeId,
        "Gender",
        {"Male", "Female"}
    }}
};

static const std::map<std::string, ColorType> kColorTypesById = {
    {kStdColorsTypeId, {
        kStdColorsTypeId,
        "Base Colors",
        {
            {"Black", "#000000"},
            {"White", "#FFFFFF"},
            {"Gray", "#808080"},
            {"Red", "#FF0000"},
            {"Orange", "#FFA500"},
            {"Yellow", "#FFFF00"},
            {"Violet", "#800080"},
            {"Green", "#008000"},
            {"Blue", "#0000FF"},
            {"Brown", "#A52A2A"}
        }
    }}
};

static std::string randomTypeId(const std::set<std::string>& objectTypeIds)
{
    if (objectTypeIds.empty())
        return {};

    const int advanceStepsCount = rand() % objectTypeIds.size();
    auto it = objectTypeIds.cbegin();
    std::advance(it, advanceStepsCount);
    return *it;
}

Module::Module():
    m_attributeGenerator(std::make_unique<AttributeGenerator>(
        &kObjectTypesById,
        &kEnumTypesById,
        &kColorTypesById))
{
    m_attributeGenerator->registerCustomAttributeGenerator(
        "Make", []() { return generateVehicleVendor(); });

    m_attributeGenerator->registerCustomAttributeGenerator(
        "Model", []() { return generateVehicleModel(); });

    m_attributeGenerator->registerCustomAttributeGenerator(
        "Number", []() { return generateLicensePlate(); });
}

std::vector<std::string> Module::settingsSections() const
{
    using namespace nx::kit;

    Json::object json;
    json.insert(std::make_pair("type", "Section"));
    json.insert(std::make_pair("caption", "Standard Analytics Taxonomy"));

    Json::object groupBox;
    groupBox.insert(std::make_pair("type", "GroupBox"));
    groupBox.insert(std::make_pair("caption", "Object generation settings"));

    Json::array itemArray;
    for (const auto& objectType: kObjectTypesById)
    {
        Json::object itemJson;
        itemJson.insert(std::make_pair("type", "CheckBox"));
        itemJson.insert(std::make_pair("caption", "Generate " + objectType.first));
        itemJson.insert(std::make_pair("name", objectType.first + ".generate"));
        itemJson.insert(std::make_pair("defaultValue", false));

        itemArray.push_back(std::move(itemJson));
    }

    groupBox.insert(std::make_pair("items", std::move(itemArray)));

    Json::array outerItemArray;
    outerItemArray.push_back(std::move(groupBox));

    json.insert(std::make_pair("items", std::move(outerItemArray)));

    return { Json(json).dump() };
}

std::vector<std::string> Module::supportedObjectTypeIds() const
{
    std::vector<std::string> result;
    for (const auto& objectType: kObjectTypesById)
    {
        if (kExcludedFromSupportedTypes.find(objectType.first)
            == kExcludedFromSupportedTypes.cend())
            result.push_back(objectType.first);
    }

    return result;
}

std::vector<std::string> Module::supportedTypes() const
{
    // TODO: rewrite it in some sane way.
    std::vector<std::string> result{
        R"json(
            {
                "objectTypeId": "nx.base.Vehicle",
                "attributes": [
                    "Color",
                    "Speed",
                    "Make",
                    "License Plate.Number"
                ]
            }
        )json",
        R"json(
            {
                "objectTypeId": "nx.base.Car",
                "attributes" : [
                    "Color",
                    "Speed",
                    "Make"
                ]
            }
        )json",
        R"json(
            {
                "objectTypeId": "nx.base.Truck",
                "attributes" : [
                    "License Plate.Number"
                ]
            }
        )json",
        R"json(
            {
                "objectTypeId": "nx.base.Person",
                "attributes" : [
                    "Gender",
                    "Hat",
                    "Glasses",
                    "Mask",
                    "Bag",
                    "Top Clothing Color",
                    "Top Clothing Length",
                    "Bottom Clothing Color",
                    "Bottom Clothing Length"
                ]
            }
        )json"
    };

    return result;
}

std::vector<std::string> Module::typeLibraryObjectTypes() const
{
    std::vector<std::string> result;

    for (const auto& objectType: kObjectTypesById)
        result.push_back(objectType.second.serialize().dump());

    return result;
}

std::vector<std::string> Module::typeLibraryEnumTypes() const
{
    std::vector<std::string> result;

    for (const auto& enumType: kEnumTypesById)
        result.push_back(enumType.second.serialize().dump());

    return result;
}

std::vector<std::string> Module::typeLibraryColorTypes() const
{
    std::vector<std::string> result;

    for (const auto& colorType: kColorTypesById)
        result.push_back(colorType.second.serialize().dump());

    return result;
}

std::unique_ptr<AbstractObject> Module::generateObject() const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string objectTypeId = randomTypeId(m_objectTypeIdsToGenerate);

    if (objectTypeId.empty())
        return nullptr;

    return std::make_unique<TaxonomyObject>(
        objectTypeId,
        m_attributeGenerator->generateAttributes(objectTypeId));
}

bool Module::needToGenerateObjects() const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return !m_objectTypeIdsToGenerate.empty();
}

void Module::setSettings(std::map<std::string, std::string> settings)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_objectTypeIdsToGenerate.clear();
    for (const auto& objectType: kObjectTypesById)
    {
        const auto it = settings.find(objectType.first + ".generate");
        if (it == settings.cend())
            continue;

        if (toBool(it->second))
            m_objectTypeIdsToGenerate.insert(objectType.first);
    }
}


} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx