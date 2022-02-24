// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

#include "attribute_generator.h"

#include <nx/vms_server_plugins/analytics/stub/deprecated_object_detection/objects/abstract_object.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace modules {
namespace standard_taxonomy {

class Module
{
public:
    Module();

    std::vector<std::string> settingsSections() const;

    std::vector<std::string> supportedObjectTypeIds() const;

    std::vector<std::string> supportedTypes() const;

    std::vector<std::string> typeLibraryObjectTypes() const;

    std::vector<std::string> typeLibraryEnumTypes() const;

    std::vector<std::string> typeLibraryColorTypes() const;

    std::unique_ptr<AbstractObject> generateObject() const;

    bool needToGenerateObjects() const;

    void setSettings(std::map<std::string, std::string> settings);

private:
    mutable std::mutex m_mutex;

    std::unique_ptr<AttributeGenerator> m_attributeGenerator;

    std::set<std::string> m_objectTypeIdsToGenerate;
};

} // namespace standard_taxonomy
} // namespace modules
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx