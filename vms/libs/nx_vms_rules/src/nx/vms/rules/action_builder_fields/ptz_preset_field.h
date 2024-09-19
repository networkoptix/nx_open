// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"

namespace nx::vms::rules {

/** Stores PTZ preset id, displayed as combobox. */
class NX_VMS_RULES_API PtzPresetField: public SimpleTypeActionField<QString, PtzPresetField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.ptzPreset")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<QString, PtzPresetField>::SimpleTypeActionField;

    static QJsonObject openApiDescriptor(const QVariantMap& properties)
    {
        auto descriptor = SimpleTypeField::openApiDescriptor(properties);
        descriptor[utils::kDescriptionProperty] =
            "To find the required Ptz Preset, "
            "refer to the response of the request to /rest/v4/devices/{deviceId}/ptz/presets.";
        return descriptor;
    }

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
