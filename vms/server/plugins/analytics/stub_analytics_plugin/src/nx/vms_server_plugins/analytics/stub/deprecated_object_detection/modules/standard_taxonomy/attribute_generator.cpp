// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "attribute_generator.h"

#include <random>
#include <algorithm>

#include <nx/kit/utils.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

static int randomInteger(const SimpleOptional<int>& minValue, const SimpleOptional<int>& maxValue)
{
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(
        minValue ? *minValue : std::numeric_limits<int>::min(),
        maxValue ? *maxValue : std::numeric_limits<int>::max());

    return distribution(generator);
}

static double randomReal(
    const SimpleOptional<double>& minValue,
    const SimpleOptional<double>& maxValue)
{
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_real_distribution<> distribution(
        minValue ? *minValue : std::numeric_limits<double>::min(),
        maxValue ? *maxValue : std::numeric_limits<double>::max());

    return distribution(generator);
}

static std::string randomColor(const std::vector<ColorType::Item>& items)
{
    if (items.empty())
        return {};

    return items[rand() % items.size()].name;
}

static std::string randomEnumItem(const std::vector<std::string>& enumItems)
{
    if (enumItems.empty())
        return {};

    return enumItems[rand() % enumItems.size()];
}

AttributeGenerator::AttributeGenerator(
    const std::map<std::string, ObjectType>* objectTypeById,
    const std::map<std::string, EnumType>* enumTypeById,
    const std::map<std::string, ColorType>* colorTypeById)
    :
    m_objectTypeById(objectTypeById),
    m_enumTypeById(enumTypeById),
    m_colorTypeById(colorTypeById)
{
}

AttributeGenerator::~AttributeGenerator()
{
}

void AttributeGenerator::registerCustomAttributeGenerator(
    std::string attributeName,
    std::function<std::string()> generator)
{
    m_customAttributeGenerators.emplace(std::move(attributeName), std::move(generator));
}

std::map<std::string, std::string> AttributeGenerator::generateAttributes(
    const std::string& objectTypeId) const
{
    const auto it = m_objectTypeById->find(objectTypeId);
    if (it == m_objectTypeById->cend())
        return {};

    std::map<std::string, std::string> result;

    for (const Attribute& attribute: it->second.attributes)
    {
        const auto customGeneratorIt = m_customAttributeGenerators.find(attribute.name);
        if (customGeneratorIt != m_customAttributeGenerators.cend())
        {
            result[attribute.name] = customGeneratorIt->second();
        }
        else if (attribute.type == "Number")
        {
            if (attribute.subtype == "Float")
            {
                result[attribute.name] =
                    nx::kit::utils::toString(randomReal(attribute.minValue, attribute.maxValue));
            }
            else
            {
                result[attribute.name] =
                    nx::kit::utils::toString(randomInteger(attribute.minValue, attribute.maxValue));
            }
        }
        else if (attribute.type == "Enum")
        {
            const auto it = m_enumTypeById->find(attribute.subtype);
            if (it == m_enumTypeById->cend())
                continue;

            result[attribute.name] = randomEnumItem(it->second.items);
        }
        else if (attribute.type == "Color")
        {
            const auto it = m_colorTypeById->find(attribute.subtype);
            if (it == m_colorTypeById->cend())
                continue;

            result[attribute.name] = randomColor(it->second.items);
        }
        else if (attribute.type == "Object")
        {
            std::map<std::string, std::string> nestedAttributes =
                generateAttributes(attribute.subtype);
            for (const auto& nestedNameAndValue: nestedAttributes)
                result[attribute.name + "." + nestedNameAndValue.first] = nestedNameAndValue.second;
        }
    }

    if (!it->second.base.empty())
    {
        std::map<std::string, std::string> baseAttributes = generateAttributes(it->second.base);
        for (const auto& baseNameAndValue: baseAttributes)
            result[baseNameAndValue.first] = baseNameAndValue.second;
    }

    return result;
}

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
