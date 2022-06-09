// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::analytics::taxonomy {

NX_VMS_COMMON_API extern const QString kIntegerAttributeSubtype;
NX_VMS_COMMON_API extern const QString kFloatAttributeSubtype;

NX_VMS_COMMON_API extern const QString kPluginDescriptorTypeName;
NX_VMS_COMMON_API extern const QString kEngineDescriptorTypeName;
NX_VMS_COMMON_API extern const QString kGroupDescriptorTypeName;
NX_VMS_COMMON_API extern const QString kEnumTypeDescriptorTypeName;
NX_VMS_COMMON_API extern const QString kColorTypeDescriptorTypeName;
NX_VMS_COMMON_API extern const QString kObjectTypeDescriptorTypeName;
NX_VMS_COMMON_API extern const QString kEventTypeDescriptorTypeName;

struct ProcessingError
{
    QString details;
};

} // namespace nx::analytics::taxonomy
