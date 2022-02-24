// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

namespace nx::analytics::taxonomy {

extern const QString kIntegerAttributeSubtype;
extern const QString kFloatAttributeSubtype;

extern const QString kPluginDescriptorTypeName;
extern const QString kEngineDescriptorTypeName;
extern const QString kGroupDescriptorTypeName;
extern const QString kEnumTypeDescriptorTypeName;
extern const QString kColorTypeDescriptorTypeName;
extern const QString kObjectTypeDescriptorTypeName;
extern const QString kEventTypeDescriptorTypeName;

struct ProcessingError
{
    QString details;
};

} // namespace nx::analytics::taxonomy
