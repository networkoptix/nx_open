// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "int_field.h"

namespace nx::vms::rules {

/**
 * Stores frames-per-second for recording Devices.
 * Max value is updated dynamically, depending on a selected Device.
 * Displayed as a spinbox.
 */
class NX_VMS_RULES_API FpsField: public SimpleTypeActionField<int, FpsField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "fps")

    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<int, FpsField>::SimpleTypeActionField;

    static QJsonObject openApiDescriptor(const QVariantMap& properties)
    {
        auto descriptor = SimpleTypeActionField::openApiDescriptor(properties);
        descriptor[utils::kDescriptionProperty] = "If the value is set to 0, "
            "the maximum available FPS for the recording device will be used.";
        return descriptor;
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
