// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <functional>

#include "enum_type.h"
#include "color_type.h"
#include "object_type.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

class AttributeGenerator
{
public:
    AttributeGenerator(
        const std::map<std::string, ObjectType>* objectTypeById,
        const std::map<std::string, EnumType>* enumTypeById,
        const std::map<std::string, ColorType>* colorTypeById);

    ~AttributeGenerator();

    void registerCustomAttributeGenerator(
        std::string attributeName,
        std::function<std::string()> generator);

    std::map<std::string, std::string> generateAttributes(const std::string& objectTypeId) const;

private:
    const std::map<std::string, ObjectType>* m_objectTypeById = nullptr;
    const std::map<std::string, EnumType>* m_enumTypeById = nullptr;
    const std::map<std::string, ColorType>* m_colorTypeById = nullptr;

    std::map<std::string, std::function<std::string()>> m_customAttributeGenerators;
};

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx