// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <QtCore/QString>

namespace nx::analytics::taxonomy {

NX_VMS_COMMON_API extern const std::string kIntegerAttributeSubtype;
NX_VMS_COMMON_API extern const std::string kFloatAttributeSubtype;

NX_VMS_COMMON_API extern const std::string kPluginDescriptorTypeName;
NX_VMS_COMMON_API extern const std::string kEngineDescriptorTypeName;
NX_VMS_COMMON_API extern const std::string kGroupDescriptorTypeName;
NX_VMS_COMMON_API extern const std::string kEnumTypeDescriptorTypeName;
NX_VMS_COMMON_API extern const std::string kColorTypeDescriptorTypeName;
NX_VMS_COMMON_API extern const std::string kObjectTypeDescriptorTypeName;
NX_VMS_COMMON_API extern const std::string kEventTypeDescriptorTypeName;

struct ProcessingError
{
    QString details;
};

} // namespace nx::analytics::taxonomy
